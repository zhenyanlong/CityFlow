#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVehicleActor, Log, All);

AVehicleActor::AVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	VehicleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleRoot"));
	SetRootComponent(VehicleRoot);

	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(VehicleRoot);
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetAbsolute(true, true, true);
	PathSpline->SetWorldLocation(FVector::ZeroVector);
	PathSpline->SetVisibility(false);
	PathSpline->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackCube(TEXT("/Engine/BasicShapes/Cube"));
	if (FallbackCube.Succeeded())
	{
		VehicleMesh->SetStaticMesh(FallbackCube.Object);
		VehicleMesh->SetRelativeScale3D(FVector(0.4f, 0.2f, 0.15f));
	}
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();
}

void AVehicleActor::SetSplinePath(const TArray<FVector>& WorldPoints, const TArray<FVector>& TangentDirs,
	const TArray<float>& ArriveTangentLengths, const TArray<float>& LeaveTangentLengths,
	float DefaultTangentLength)
{
	PathSpline->ClearSplinePoints();
	PathSpline->SetWorldLocation(FVector::ZeroVector);

	const int32 NumPts = WorldPoints.Num();
	if (NumPts < 2 || TangentDirs.Num() != NumPts)
	{
		return;
	}

	const bool bHasLeave = (LeaveTangentLengths.Num() == NumPts);

	float BaseTangentLen = DefaultTangentLength;
	if (BaseTangentLen <= 0.0f)
	{
		if (UWorld* W = GetWorld())
		{
			if (UGridManager* GM = W->GetSubsystem<UGridManager>())
			{
				BaseTangentLen = GM->GetCellSize();
			}
		}
		if (BaseTangentLen <= 0.0f)
		{
			BaseTangentLen = 100.0f;
		}
	}

	// Step 1: Old way — add points and set uniform tangents (both arrive & leave = same)
	for (int32 i = 0; i < NumPts; ++i)
	{
		PathSpline->AddSplineWorldPoint(WorldPoints[i]);
	}
	for (int32 i = 0; i < NumPts; ++i)
	{
		const FVector Tangent = TangentDirs[i].GetSafeNormal() * BaseTangentLen;
		PathSpline->SetTangentAtSplinePoint(i, Tangent, ESplineCoordinateSpace::Local);
	}

	// Step 2: Break handle linkage — set all points to CurveCustomTangent
	for (int32 i = 0; i < NumPts; ++i)
	{
		PathSpline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);
	}
	PathSpline->UpdateSpline();

	// Step 3: Override per-segment tangents for turn curves
	// For each pair (i, i+1), both the leave tangent of point i
	// and the arrive tangent of point i+1 are scaled by the same multiplier,
	// taken from point i's LeaveTangentLength (LMult).
	// This ensures the curve segment between entry/exit offsets is symmetric.
	FInterpCurveVector& PosCurve = PathSpline->SplineCurves.Position;
	for (int32 i = 0; i < NumPts - 1; ++i)
	{
		const float LMult = bHasLeave ? LeaveTangentLengths[i] : 1.0f;
		if (FMath::IsNearlyEqual(LMult, 1.0f))
		{
			continue;
		}

		const FVector DirI   = TangentDirs[i].GetSafeNormal();
		const FVector DirI1  = TangentDirs[i + 1].GetSafeNormal();

		// Leave tangent of point i   — faces toward point i+1
		PosCurve.Points[i].LeaveTangent = DirI * BaseTangentLen * LMult;
		// Arrive tangent of point i+1 — faces toward point i
		PosCurve.Points[i + 1].ArriveTangent = DirI1 * BaseTangentLen * LMult;
	}
	PathSpline->UpdateSpline();

	CurrentSplineDistance = 0.0f;
	CurrentSpeed = 0.0f;
	MovementState = EVehicleMovementState::Moving;

	SetActorLocation(WorldPoints[0] + FVector(0, 0, VehicleZOffset));

	if (UWorld* W = GetWorld())
	{
		if (UGridManager* GridMgr = W->GetSubsystem<UGridManager>())
		{
			PreviousGridPosition = GridMgr->WorldToGrid(WorldPoints[0]);
		}
	}
}

void AVehicleActor::SetDestination(ABuilding* InDestination)
{
	Destination = InDestination;
}

void AVehicleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (MovementState)
	{
	case EVehicleMovementState::Moving:
	case EVehicleMovementState::WaitingCongestion:
		TickMovementSpline(DeltaTime);
		break;
	case EVehicleMovementState::Arrived:
	case EVehicleMovementState::Idle:
		break;
	}
}

void AVehicleActor::TickMovementSpline(float DeltaTime)
{
	if (PathSpline->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	const float SplineLength = PathSpline->GetSplineLength();
	if (CurrentSplineDistance >= SplineLength)
	{
		HandleArrival();
		return;
	}

	// =========================================================
	//  Unified forward probe: physical sweep AND intersection-
	//  lock check merged into one pass. Any obstacle ahead
	//  triggers a smooth brake via WaitingCongestion.
	// =========================================================
	PerformForwardProbe();

	if (bFrontVehicleTooClose)
	{
		MovementState = EVehicleMovementState::WaitingCongestion;
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, StartDeceleration);
		if (CurrentSpeed < 1.0f) { CurrentSpeed = 0.0f; }
		if (!VelocityDirection.IsNearlyZero())
		{
			VehicleRoot->SetWorldRotation(VelocityDirection.Rotation());
		}
		return;
	}

	// Obstacle cleared — resume
	if (MovementState == EVehicleMovementState::WaitingCongestion)
	{
		MovementState = EVehicleMovementState::Moving;
	}

	// =========================================================
	//  Speed computation: terminal deceleration + start boost
	// =========================================================
	{
		const float RemainingDist = SplineLength - CurrentSplineDistance;
		const float SpeedRatio = (DecelerationDistance > 0.0f)
			? FMath::Min(RemainingDist / DecelerationDistance, 1.0f) : 1.0f;
		const float BaseTarget = MoveSpeed * SpeedRatio;
		const float Accel = (BaseTarget > CurrentSpeed + 100.0f) ? StartAcceleration : Acceleration;
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, BaseTarget, DeltaTime, Accel);
	}

	// =========================================================
	//  Advance spline position
	// =========================================================
	CurrentSplineDistance += FMath::Min(CurrentSpeed * DeltaTime, SplineLength - CurrentSplineDistance);

	if (CurrentSplineDistance >= SplineLength)
	{
		HandleArrival();
		return;
	}

	const FVector NewPos = PathSpline->GetLocationAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);
	const FVector MoveDir = PathSpline->GetDirectionAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);

	if (!MoveDir.IsNearlyZero())
	{
		VelocityDirection = MoveDir;
		VehicleRoot->SetWorldRotation(MoveDir.Rotation());
	}

	SetActorLocation(NewPos + FVector(0, 0, VehicleZOffset));

	UWorld* World = GetWorld();
	UGridManager* GM = World ? World->GetSubsystem<UGridManager>() : nullptr;
	UVehicleManager* VM = World ? World->GetSubsystem<UVehicleManager>() : nullptr;

	PreviousGridPosition = GM ? GM->WorldToGrid(NewPos) : FGridVector();

	// =========================================================
	//  Acquire intersection lock (entered intersection cell)
	// =========================================================
	if (GM && VM)
	{
		const FGridVector NewGrid = GM->WorldToGrid(GetActorLocation());
		const FGridCell& Cell = GM->GetCell(NewGrid);
		if (Cell.Type == ECellType::Road)
		{
			int32 ConnCount = 0;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Up)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Down)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Left)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;
			if (ConnCount >= 3)
			{
				const float CellSize = GM->GetCellSize();
				const float ExitLookDist = FMath::Min(CurrentSplineDistance + CellSize, SplineLength - 1.0f);
				const FVector ExitPos = PathSpline->GetLocationAtDistanceAlongSpline(ExitLookDist, ESplineCoordinateSpace::World);
				const FGridVector ExitGrid = GM->WorldToGrid(ExitPos);
				const EGridDirection EntryDir = GridDirectionUtils::DirectionFromGridDelta(NewGrid - PreviousGridPosition);
				const EGridDirection ExitDir = (ExitGrid != NewGrid)
					? GridDirectionUtils::DirectionFromGridDelta(ExitGrid - NewGrid)
					: EGridDirection::None;
				VM->AcquireIntersectionLock(NewGrid, this, EntryDir, ExitDir);
			}
		}
	}
}

void AVehicleActor::SetPathIntersections(const TArray<FGridVector>& GridPath)
{
	PathIntersectionCells.Reset();

	UWorld* World = GetWorld();
	UGridManager* GM = World ? World->GetSubsystem<UGridManager>() : nullptr;
	if (!GM) { return; }

	for (const FGridVector& Cell : GridPath)
	{
		if (GM->GetCellType(Cell) != ECellType::Road) { continue; }

		int32 ConnCount = 0;
		const FGridCell& C = GM->GetCell(Cell);
		if (C.ConnectedMask & static_cast<uint8>(EGridDirection::Up))    ++ConnCount;
		if (C.ConnectedMask & static_cast<uint8>(EGridDirection::Down))  ++ConnCount;
		if (C.ConnectedMask & static_cast<uint8>(EGridDirection::Left))  ++ConnCount;
		if (C.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;

		if (ConnCount >= 3)
		{
			PathIntersectionCells.Add(Cell);
		}
	}
}

void AVehicleActor::PerformForwardProbe()
{
	FrontVehicle.Reset();
	FrontVehicleDistance = 0.0f;
	bFrontVehicleTooClose = false;

	if (VelocityDirection.IsNearlyZero())
	{
		return;
	}

	UWorld* World = GetWorld();
	UGridManager* GM = World ? World->GetSubsystem<UGridManager>() : nullptr;
	UVehicleManager* VM = World ? World->GetSubsystem<UVehicleManager>() : nullptr;
	if (!GM || !VM)
	{
		return;
	}

	const FVector MyDir = VelocityDirection.GetSafeNormal();
	const FVector BasePos = GetActorLocation() + FVector(0, 0, ProbeVerticalOffset);
	const FVector ProbeStart = BasePos + MyDir * SelfAvoidOffset;
	const FVector ProbeEnd = ProbeStart + MyDir * ForwardProbeDistance;
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(ForwardProbeRadius);

	// ---- Physical sphere sweep ----
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, ProbeStart, ProbeEnd, FQuat::Identity,
		ECC_GameTraceChannel1, ProbeShape, QueryParams);

	AVehicleActor* ClosestVehicle = nullptr;
	float ClosestDist = ForwardProbeDistance;

	for (const FHitResult& Hit : Hits)
	{
		AVehicleActor* OtherVehicle = Cast<AVehicleActor>(Hit.GetActor());
		if (!OtherVehicle || OtherVehicle == this) { continue; }

		const float ProjDist = FVector::DotProduct(Hit.ImpactPoint - ProbeStart, MyDir);
		if (ProjDist > 0.0f && ProjDist < ClosestDist)
		{
			ClosestDist = ProjDist;
			ClosestVehicle = OtherVehicle;
		}
	}

	// ---- Path intersection lookup: pre-stored from A* grid data ----
	for (const FGridVector& InterCell : PathIntersectionCells)
	{
		const FVector CellWorldPos = GM->GridToWorld(InterCell);
		const FVector ToCell = CellWorldPos - ProbeStart;
		const float CellDist = FVector::DotProduct(ToCell, MyDir);

		if (CellDist <= 0.0f || CellDist > ForwardProbeDistance) { continue; }

		if (!VM->IsIntersectionLockedByOther(InterCell, this)) { continue; }

		if (CellDist < ClosestDist)
		{
			ClosestDist = CellDist;
			ClosestVehicle = nullptr; // Virtual obstacle, no physical vehicle
		}
	}

	// ---- Combine results ----
	if (ClosestDist < ForwardProbeDistance)
	{
		FrontVehicle        = ClosestVehicle;
		FrontVehicleDistance = ClosestDist;

		if (ClosestVehicle)
		{
			// Physical vehicle: maintain safe following distance
			const float SafeDist = FMath::Max(SafeDistanceMin, CurrentSpeed * SafeDistanceSeconds);
			bFrontVehicleTooClose = (FrontVehicleDistance <= SafeDist);
		}
		else
		{
			// Virtual obstacle (locked intersection): always stop
			bFrontVehicleTooClose = true;
		}
	}

	// ---- Debug drawing ----
	if (bDebugDrawProbe)
	{
		DrawDebugSphere(World, ProbeStart, 10.0f, 8, FColor::Green, false, -1.0f, 0, 2.0f);

		const int32 NumSegments = FMath::Max(1, FMath::CeilToInt(ForwardProbeDistance / ForwardProbeRadius));
		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Alpha = static_cast<float>(i) / NumSegments;
			const FVector Pos = FMath::Lerp(ProbeStart, ProbeEnd, Alpha);
			const FColor SphereColor = (i == NumSegments) ? FColor::Cyan : FColor(0, 128, 128);
			DrawDebugSphere(World, Pos, ForwardProbeRadius, 12, SphereColor, false, -1.0f, 0, 0.5f);
		}
		DrawDebugLine(World, ProbeStart, ProbeEnd, FColor::Cyan, false, -1.0f, 0, 1.0f);

		if (ClosestVehicle || ClosestDist < ForwardProbeDistance)
		{
			const float DebugSafeDist = FMath::Max(SafeDistanceMin, CurrentSpeed * SafeDistanceSeconds);
			const FVector SafeBoundaryPos = ProbeStart + MyDir * DebugSafeDist;
			DrawDebugSphere(World, SafeBoundaryPos, 15.0f, 8, FColor::Yellow, false, -1.0f, 0, 2.0f);

			const FColor VehColor = bFrontVehicleTooClose ? FColor::Red : FColor::Orange;
			if (ClosestVehicle)
			{
				DrawDebugLine(World, ProbeStart, ClosestVehicle->GetActorLocation(), VehColor, false, -1.0f, 0, 2.0f);
				DrawDebugSphere(World, ClosestVehicle->GetActorLocation(), 30.0f, 8, VehColor, false, -1.0f, 0, 2.0f);
			}

			const FVector MidPt = ProbeStart + MyDir * (ClosestDist * 0.5f);
			DrawDebugString(World, MidPt + FVector(0, 0, 100.0f),
				FString::Printf(TEXT("%.0f / %.0f%s"), ClosestDist, DebugSafeDist,
					ClosestVehicle ? TEXT("") : TEXT(" INT")),
				nullptr, VehColor, 0.0f, true);
		}
	}
}

void AVehicleActor::HandleArrival()
{
	MovementState = EVehicleMovementState::Arrived;
	VelocityDirection = FVector::ZeroVector;
	OnVehicleArrived.Broadcast(this);
}

void AVehicleActor::SetDebugColor(FLinearColor Color)
{
	DebugColor = Color;

	if (VehicleMesh && VehicleMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynMat = VehicleMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
}

void AVehicleActor::SetWaitingForIntersection(bool bWaiting)
{
	// Deprecated: WaitingIntersection state is no longer used.
	// All stopping is now handled by the unified forward probe.
}
