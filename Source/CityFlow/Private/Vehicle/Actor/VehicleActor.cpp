#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVehicleActor, Log, All);

AVehicleActor::AVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	VehicleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleRoot"));
	SetRootComponent(VehicleRoot);

	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(VehicleRoot);
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
		TickMovementSpline(DeltaTime);
		break;
	case EVehicleMovementState::WaitingIntersection:
		IntersectionWaitTimer -= DeltaTime;
		if (IntersectionWaitTimer <= 0.0f)
		{
			MovementState = EVehicleMovementState::Moving;
		}
		break;
	case EVehicleMovementState::WaitingCongestion:
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

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGridManager* GM = World->GetSubsystem<UGridManager>();
	UVehicleManager* VM = World->GetSubsystem<UVehicleManager>();

	if (GM && VM)
	{
		const float CellSize = GM->GetCellSize();
		const float LookAhead = CellSize * 0.5f;
		const float LookDist = FMath::Min(CurrentSplineDistance + LookAhead, SplineLength - 1.0f);

		const FVector AheadPos = PathSpline->GetLocationAtDistanceAlongSpline(LookDist, ESplineCoordinateSpace::World);
		const FGridVector AheadGrid = GM->WorldToGrid(AheadPos);
		const FGridVector CurrentGrid = GM->WorldToGrid(GetActorLocation());

		if (AheadGrid != CurrentGrid)
		{
			const FGridCell& AheadCell = GM->GetCell(AheadGrid);
			if (AheadCell.Type == ECellType::Road)
			{
				int32 ConnCount = 0;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Up)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Down)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Left)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;

				if (ConnCount >= 3)
				{
					const EGridDirection EntryDir = GridDirectionUtils::DirectionFromWorldVector(VelocityDirection);
					if (VM->IsIntersectionLockedByOther(AheadGrid, this, EntryDir))
					{
						MovementState = EVehicleMovementState::WaitingIntersection;
						WaitingIntersectionPos = AheadGrid;
						IntersectionWaitTimer = IntersectionWaitTime;
						return;
					}
				}
			}
		}
	}

	{
		const float RemainingDist = SplineLength - CurrentSplineDistance;
		const float SpeedRatio = (DecelerationDistance > 0.0f)
			? FMath::Min(RemainingDist / DecelerationDistance, 1.0f)
			: 1.0f;
		const float TargetSpeed = MoveSpeed * SpeedRatio;
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Acceleration);
	}

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
				const EGridDirection EntryDir = GridDirectionUtils::DirectionFromWorldVector(VelocityDirection);
				VM->AcquireIntersectionLock(NewGrid, this, EntryDir);
			}
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
	if (bWaiting)
	{
		MovementState = EVehicleMovementState::WaitingIntersection;
		IntersectionWaitTimer = IntersectionWaitTime;
	}
	else
	{
		if (MovementState == EVehicleMovementState::WaitingIntersection)
		{
			MovementState = EVehicleMovementState::Moving;
		}
	}
}
