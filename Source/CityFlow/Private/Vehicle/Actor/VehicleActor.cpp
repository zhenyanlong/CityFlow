#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Grid/GridManager.h"
#include "Grid/RoadTile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "Materials/MaterialInstanceDynamic.h"

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

void AVehicleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Release all intersection reservations so stale entries don't block other vehicles
	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	Super::EndPlay(EndPlayReason);
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

	for (int32 i = 0; i < NumPts; ++i)
	{
		PathSpline->AddSplineWorldPoint(WorldPoints[i]);
	}
	for (int32 i = 0; i < NumPts; ++i)
	{
		const FVector Tangent = TangentDirs[i].GetSafeNormal() * BaseTangentLen;
		PathSpline->SetTangentAtSplinePoint(i, Tangent, ESplineCoordinateSpace::Local);
	}

	for (int32 i = 0; i < NumPts; ++i)
	{
		PathSpline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);
	}
	PathSpline->UpdateSpline();

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

		PosCurve.Points[i].LeaveTangent = DirI * BaseTangentLen * LMult;
		PosCurve.Points[i + 1].ArriveTangent = DirI1 * BaseTangentLen * LMult;
	}
	PathSpline->UpdateSpline();

	// Clear any stale reservations from a previous path
	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	CurrentSplineDistance = 0.0f;
	CurrentSpeed = 0.0f;
	CongestionWaitTime = 0.0f;
	TotalStopTime = 0.0f;
	FlashMaterialInstance = nullptr;
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

	// ---- Berserk mode: skip forward probe, ram through ----
	if (!bBerserk)
	{
		PerformForwardProbe();
	}
	else
	{
		bFrontVehicleTooClose = false;
		PerformRamKill();
	}

	if (bFrontVehicleTooClose)
	{
		if (MovementState != EVehicleMovementState::WaitingCongestion)
		{
			CongestionWaitTime = 0.0f;
		}
		MovementState = EVehicleMovementState::WaitingCongestion;

		CongestionWaitTime += DeltaTime;

		// Deadlock timeout: if we've been waiting too long, release all intersection
		// reservations. This breaks the hold-and-wait cycle when two vehicles each
		// occupy one of two adjacent intersections and block each other.
		if (CongestionWaitTime >= DeadlockTimeout)
		{
			for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
			{
				if (ARoadTile* Tile = WeakTile.Get())
				{
					Tile->ReleaseVehicleFromAllTables(this);
				}
			}
			ReservedIntersections.Empty();
			PassedIntersections.Empty();
			CongestionWaitTime = 0.0f;

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange,
					FString::Printf(TEXT("[%s] Deadlock timeout — released all intersection locks"),
						*GetName()));
			}
		}

		// === Death / Stop System ===
		TotalStopTime += DeltaTime;
		OnVehicleStopped(DeltaTime);

		if (TotalStopTime >= DeathTimeout)
		{
			HandleWaitTimeout();
			if (IsActorBeingDestroyed())
			{
				return; // Vehicle died
			}
			// Vehicle didn't die (e.g., entered berserk mode) — fall through to movement
			bFrontVehicleTooClose = false;
		}
		else
		{
			CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, 0.0f, DeltaTime, StartDeceleration);
			if (CurrentSpeed < 1.0f) { CurrentSpeed = 0.0f; }
			if (!VelocityDirection.IsNearlyZero())
			{
				VehicleRoot->SetWorldRotation(VelocityDirection.Rotation());
			}
			return;
		}
	}

	if (MovementState == EVehicleMovementState::WaitingCongestion)
	{
		MovementState = EVehicleMovementState::Moving;
		CongestionWaitTime = 0.0f;
		OnVehicleResumed();
		if (ShouldResetStopTime())
		{
			TotalStopTime = 0.0f;
		}
	}

	{
		const float RemainingDist = SplineLength - CurrentSplineDistance;
		const float SpeedRatio = (DecelerationDistance > 0.0f)
			? FMath::Min(RemainingDist / DecelerationDistance, 1.0f) : 1.0f;
		const float EffectiveMoveSpeed = MoveSpeed * GetBerserkSpeedMultiplier();
		const float BaseTarget = EffectiveMoveSpeed * SpeedRatio;
		const float Accel = (BaseTarget > CurrentSpeed + 100.0f) ? StartAcceleration : Acceleration;
		CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, BaseTarget, DeltaTime, Accel);
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

	UWorld* World = GetWorld();
	UGridManager* GM = World ? World->GetSubsystem<UGridManager>() : nullptr;
	PreviousGridPosition = GM ? GM->WorldToGrid(NewPos) : FGridVector();
}

void AVehicleActor::PerformForwardProbe()
{
	FrontVehicle.Reset();
	FrontVehicleDistance = 0.0f;
	bFrontVehicleTooClose = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ProbeStart;
	FVector ProbeEnd;
	FVector MyDir;
	if (!BuildForwardProbeSegment(ProbeStart, ProbeEnd, MyDir))
	{
		return;
	}

	const float ProbeSegmentDistance = FVector::Distance(ProbeStart, ProbeEnd);
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(ForwardProbeRadius);

	// Derive grid entry direction from our current velocity
	const EGridDirection EntryDir = GridDirectionUtils::DirectionFromWorldVector(MyDir);

	// ---- Pass 1: Physical vehicle sweep (Channel1 = Vehicle) ----
	FCollisionQueryParams VehQueryParams;
	VehQueryParams.bTraceComplex = false;
	VehQueryParams.AddIgnoredActor(this);

	TArray<FHitResult> VehHits;
	World->SweepMultiByChannel(VehHits, ProbeStart, ProbeEnd, FQuat::Identity,
		ECC_GameTraceChannel1, ProbeShape, VehQueryParams);

	AVehicleActor* ClosestVehicle = nullptr;
	float ClosestDist = ProbeSegmentDistance;

	for (const FHitResult& Hit : VehHits)
	{
		AVehicleActor* OtherVehicle = Cast<AVehicleActor>(Hit.GetActor());
		if (!OtherVehicle || OtherVehicle == this) { continue; }

		const float ProjDist = FVector::DotProduct(Hit.ImpactPoint - ProbeStart, MyDir);
		if (ProjDist >= 0.0f && ProjDist < ClosestDist)
		{
			ClosestDist = ProjDist;
			ClosestVehicle = OtherVehicle;
		}
	}

	// ---- Pass 2: Intersection box sweep (Channel2 = Intersection) ----
	FCollisionQueryParams InterQueryParams;
	InterQueryParams.bTraceComplex = false;
	InterQueryParams.AddIgnoredActor(this);

	TArray<FHitResult> InterHits;
	World->SweepMultiByChannel(InterHits, ProbeStart, ProbeEnd, FQuat::Identity,
		ECC_GameTraceChannel2, ProbeShape, InterQueryParams);

	for (const FHitResult& Hit : InterHits)
	{
		ARoadTile* RoadTile = Cast<ARoadTile>(Hit.GetActor());
		if (!RoadTile || !RoadTile->IsIntersection())
		{
			continue;
		}

		const float InterDist = FVector::DotProduct(Hit.ImpactPoint - ProbeStart, MyDir);
		if (InterDist < 0.0f || InterDist > ProbeSegmentDistance)
		{
			continue;
		}

		if (RoadTile->TryAcquireIntersectionLock(this, EntryDir))
		{
			ReservedIntersections.AddUnique(RoadTile);
			continue;
		}

		// Lock rejected → treat as virtual obstacle
		if (InterDist < ClosestDist)
		{
			ClosestDist = InterDist;
			ClosestVehicle = nullptr;
		}
	}

	// ---- Combine results ----
	if (ClosestDist < ProbeSegmentDistance)
	{
		FrontVehicle = ClosestVehicle;
		FrontVehicleDistance = ClosestDist;

		if (ClosestVehicle)
		{
			const float SafeDist = FMath::Max(SafeDistanceMin, CurrentSpeed * SafeDistanceSeconds);
			bFrontVehicleTooClose = (FrontVehicleDistance <= SafeDist);
		}
		else
		{
			bFrontVehicleTooClose = true;
		}
	}

	// ---- Debug drawing ----
	if (bDebugDrawProbe)
	{
		DrawDebugSphere(World, ProbeStart, 10.0f, 8, FColor::Green, false, -1.0f, 0, 2.0f);

		const int32 NumSegments = FMath::Max(1, FMath::CeilToInt(ProbeSegmentDistance / ForwardProbeRadius));
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
	// Release all intersection reservations
	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	MovementState = EVehicleMovementState::Arrived;
	VelocityDirection = FVector::ZeroVector;
	OnVehicleArrived.Broadcast(this);
}

void AVehicleActor::MarkIntersectionPassed(ARoadTile* Tile)
{
	if (Tile)
	{
		PassedIntersections.Add(Tile);
	}
}

bool AVehicleActor::HasPassedIntersection(ARoadTile* Tile) const
{
	if (!Tile) return false;
	return PassedIntersections.Contains(TWeakObjectPtr<ARoadTile>(Tile));
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

// ===== Death / Stop System =====

static const FName FlashIntensityParamName(TEXT("FlashIntensity"));

void AVehicleActor::OnVehicleStopped(float DeltaTime)
{
	if (!VehicleMesh)
	{
		return;
	}

	// Create dynamic material instance lazily on first stop
	if (!FlashMaterialInstance)
	{
		UMaterialInterface* BaseMat = VehicleMesh->GetMaterial(0);
		if (BaseMat)
		{
			FlashMaterialInstance = VehicleMesh->CreateAndSetMaterialInstanceDynamic(0);
		}
	}

	if (FlashMaterialInstance)
	{
		// Flash frequency increases with stop duration: 0.5 Hz → 4.0 Hz
		const float Progress = FMath::Clamp(TotalStopTime / FMath::Max(DeathTimeout, 1.0f), 0.0f, 1.0f);
		const float Freq = FMath::Lerp(0.5f, 4.0f, Progress);

		// Pulsing red emissive: sin wave 0→1
		const float Intensity = (FMath::Sin(TotalStopTime * Freq * 2.0f * PI) + 1.0f) * 0.5f;
		FlashMaterialInstance->SetScalarParameterValue(FlashIntensityParamName, Intensity);
	}
}

void AVehicleActor::OnVehicleResumed()
{
	if (FlashMaterialInstance)
	{
		FlashMaterialInstance->SetScalarParameterValue(FlashIntensityParamName, 0.0f);
	}
}

void AVehicleActor::HandleVehicleDeath()
{
	const FVector DeathLocation = GetActorLocation();

	// 1. Release all intersection reservations
	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	// 2. Spawn explosion VFX (one-shot, auto-destroy on finish)
	if (ExplosionVFX)
	{
		UNiagaraComponent* VFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionVFX, DeathLocation, FRotator::ZeroRotator,
			FVector::OneVector, true, true, ENCPoolMethod::None);
		if (VFXComp)
		{
			if (!ExplosionVFXScaleParamName.IsNone())
			{
				VFXComp->SetVariableFloat(ExplosionVFXScaleParamName, ExplosionVFXScale);
			}
		}
	}

	// 3. Play explosion SFX
	if (ExplosionSFX)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSFX, DeathLocation);
	}

	// 4. Proximity-scaled camera shake
	if (DeathCameraShake)
	{
		if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			const FVector CamLoc = CamMgr->GetCameraLocation();
			const float Dist = FVector::Dist(CamLoc, DeathLocation);
			const float Scale = (DeathShakeMaxDistance > 0.0f)
				? FMath::Clamp(1.0f - Dist / DeathShakeMaxDistance, 0.0f, 1.0f)
				: 1.0f;

			if (Scale > 0.0f)
			{
				CamMgr->StartCameraShake(DeathCameraShake, Scale);
			}
		}
	}

	// 5. Broadcast death event
	OnVehicleDeath.Broadcast(this);

	// 6. Destroy this actor
	Destroy();
}

void AVehicleActor::HandleWaitTimeout()
{
	// Base: die on timeout
	HandleVehicleDeath();
}

bool AVehicleActor::BuildForwardProbeSegment(FVector& OutProbeStart, FVector& OutProbeEnd, FVector& OutDirection) const
{
	if (!PathSpline || PathSpline->GetNumberOfSplinePoints() < 2)
	{
		return false;
	}

	const float SplineLength = PathSpline->GetSplineLength();
	if (SplineLength <= 0.0f || CurrentSplineDistance >= SplineLength)
	{
		return false;
	}

	const float StartDistance = FMath::Clamp(CurrentSplineDistance + SelfAvoidOffset, 0.0f, SplineLength);
	const float EndDistance = FMath::Clamp(StartDistance + ForwardProbeDistance, 0.0f, SplineLength);
	if (EndDistance <= StartDistance)
	{
		return false;
	}

	OutProbeStart = PathSpline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World)
		+ FVector(0, 0, ProbeVerticalOffset);
	OutProbeEnd = PathSpline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::World)
		+ FVector(0, 0, ProbeVerticalOffset);

	OutDirection = (OutProbeEnd - OutProbeStart).GetSafeNormal();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = PathSpline->GetDirectionAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World).GetSafeNormal();
	}

	return !OutDirection.IsNearlyZero();
}

void AVehicleActor::PerformRamKill()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ProbeStart;
	FVector ProbeEnd;
	FVector MyDir;
	if (!BuildForwardProbeSegment(ProbeStart, ProbeEnd, MyDir))
	{
		return;
	}

	const float ProbeSegmentDistance = FVector::Distance(ProbeStart, ProbeEnd);
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(ForwardProbeRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, ProbeStart, ProbeEnd, FQuat::Identity,
		ECC_GameTraceChannel1, ProbeShape, QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		AVehicleActor* OtherVehicle = Cast<AVehicleActor>(Hit.GetActor());
		if (!OtherVehicle || OtherVehicle == this)
		{
			continue;
		}

		// Only kill vehicles that are in front (skip behind)
		const float ProjDist = FVector::DotProduct(Hit.ImpactPoint - ProbeStart, MyDir);
		if (ProjDist < 0.0f || ProjDist > ProbeSegmentDistance)
		{
			continue;
		}

		// Skip vehicles that are already dying or dead
		if (OtherVehicle->IsActorBeingDestroyed())
		{
			continue;
		}

		// Skip vehicles that have already arrived
		if (OtherVehicle->GetMovementState() == EVehicleMovementState::Arrived ||
			OtherVehicle->GetMovementState() == EVehicleMovementState::Idle)
		{
			continue;
		}

		OtherVehicle->HandleVehicleDeath();
	}
}

void AVehicleActor::KillOverlappingVehicles(float OverlapRadius)
{
	if (OverlapRadius <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	const FCollisionShape OverlapShape = FCollisionShape::MakeSphere(OverlapRadius);
	World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity,
		ECC_GameTraceChannel1, OverlapShape, QueryParams);

	TSet<AVehicleActor*> KilledVehicles;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AVehicleActor* OtherVehicle = Cast<AVehicleActor>(Overlap.GetActor());
		if (!OtherVehicle || OtherVehicle == this || KilledVehicles.Contains(OtherVehicle))
		{
			continue;
		}

		if (OtherVehicle->IsActorBeingDestroyed())
		{
			continue;
		}

		if (OtherVehicle->GetMovementState() == EVehicleMovementState::Arrived ||
			OtherVehicle->GetMovementState() == EVehicleMovementState::Idle)
		{
			continue;
		}

		KilledVehicles.Add(OtherVehicle);
		OtherVehicle->HandleVehicleDeath();
	}
}
