#include "Grid/RoadTile.h"
#include "Grid/GridManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoadTile, Log, All);

static FVector GetDirectionLocalVector(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:    return FVector(0.0f, -1.0f, 0.0f);
	case EGridDirection::Down:  return FVector(0.0f, 1.0f, 0.0f);
	case EGridDirection::Left:  return FVector(-1.0f, 0.0f, 0.0f);
	case EGridDirection::Right: return FVector(1.0f, 0.0f, 0.0f);
	default:                    return FVector::ZeroVector;
	}
}

static FString DirToString(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:    return TEXT("Up");
	case EGridDirection::Down:  return TEXT("Down");
	case EGridDirection::Left:  return TEXT("Left");
	case EGridDirection::Right: return TEXT("Right");
	default:                    return TEXT("None");
	}
}

static bool ShouldShowIntersectionScreenDebug()
{
	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	return Settings && Settings->bDebugDrawIntersections;
}

ARoadTile::ARoadTile()
{
	PlaceableType = EPlaceableType::Road;

	IntersectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("IntersectionBox"));
	IntersectionBox->SetupAttachment(RootSceneComponent);
	IntersectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// ObjectType = Vehicle so that VehicleMesh (QueryVehicle preset, which overlaps
	// with Vehicle channel) generates BeginOverlap / EndOverlap events for the lock life-cycle.
	IntersectionBox->SetCollisionObjectType(ECollisionChannel::ECC_Vehicle);
	IntersectionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	IntersectionBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
	IntersectionBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Vehicle, ECR_Overlap);
	IntersectionBox->SetGenerateOverlapEvents(true);

	IndicatorPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IndicatorPlane"));
	IndicatorPlane->SetupAttachment(RootSceneComponent);
	IndicatorPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorPlane->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMeshFinder.Succeeded())
	{
		IndicatorPlane->SetStaticMesh(PlaneMeshFinder.Object);
	}
}

void ARoadTile::BeginPlay()
{
	Super::BeginPlay();

	IntersectionBox->OnComponentBeginOverlap.AddDynamic(this, &ARoadTile::OnIntersectionBoxBeginOverlap);
	IntersectionBox->OnComponentEndOverlap.AddDynamic(this, &ARoadTile::OnIntersectionBoxEndOverlap);
}

void ARoadTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear all tables so no dangling references remain
	DirectionOccupants.Empty();
	PendingReservations.Empty();
	VehicleEntryDirs.Empty();
	PendingReservationTimestamps.Empty();
	ServingDirection = EGridDirection::None;
	ServedCount = 0;
	WaitingDirs.Empty();

	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.RemoveDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ARoadTile::OnPlacedOnGrid_Implementation()
{
	Super::OnPlacedOnGrid_Implementation();

	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.AddDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	UpdateAppearance();
}

void ARoadTile::OnRemovedFromGrid_Implementation()
{
	// Clear all intersection tables
	DirectionOccupants.Empty();
	PendingReservations.Empty();
	VehicleEntryDirs.Empty();
	PendingReservationTimestamps.Empty();
	ServingDirection = EGridDirection::None;
	ServedCount = 0;
	WaitingDirs.Empty();

	IntersectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.RemoveDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	Super::OnRemovedFromGrid_Implementation();
}

void ARoadTile::OnGridCellChanged(FGridVector CellPos, const FGridCell& NewCell)
{
	if (CellPos == GridPosition)
	{
		UpdateAppearance();
	}
}

void ARoadTile::UpdatePreviewAppearance(const FGridVector& GridPos)
{
	if (!IsPreview() || !MeshComponent)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const uint8 PredictedMask = GM->CalculateConnectedMask(GridPos);

	UStaticMesh* FoundMesh = nullptr;
	float Yaw = 0.0f;
	FVector ScaleMult = FVector(1.0f, 1.0f, 1.0f);

	if (FindMeshConfig(PredictedMask, FoundMesh, Yaw, ScaleMult) && FoundMesh)
	{
		EnsureMeshMaterialsCached(FoundMesh);

		MeshComponent->SetStaticMesh(FoundMesh);

		const int32 RotCount = FMath::RoundToInt32(Yaw / 90.0f) % 4;
		if (RotCount % 2 == 1)
		{
			Swap(ScaleMult.X, ScaleMult.Y);
		}

		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));

		const float CellSize = GM->GetCellSize();
		const float BaseScale = (ReferenceCellSize > 0.0f) ? (CellSize / ReferenceCellSize) : 1.0f;
		SetActorScale3D(ScaleMult * BaseScale);

		UMaterialInterface* MaterialToApply = IsPreviewPlacementValid() ? PreviewMaterial : InvalidPreviewMaterial;
		if (MaterialToApply)
		{
			const int32 NumSlots = MeshComponent->GetNumMaterials();
			for (int32 i = 0; i < NumSlots; ++i)
			{
				MeshComponent->SetMaterial(i, MaterialToApply);
			}
		}
	}
}

void ARoadTile::UpdateAppearance()
{
	if (!IsPlacedOnGrid() || !MeshComponent)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const FGridCell& Cell = GM->GetCell(GridPosition);
	const uint8 Mask = Cell.ConnectedMask;

	UStaticMesh* FoundMesh = nullptr;
	float Yaw = 0.0f;
	FVector ScaleMult = FVector(1.0f, 1.0f, 1.0f);

	if (FindMeshConfig(Mask, FoundMesh, Yaw, ScaleMult) && FoundMesh)
	{
		EnsureMeshMaterialsCached(FoundMesh);

		MeshComponent->SetStaticMesh(FoundMesh);

		RestoreMeshMaterials(FoundMesh);

		const int32 RotCount = FMath::RoundToInt32(Yaw / 90.0f) % 4;
		if (RotCount % 2 == 1)
		{
			Swap(ScaleMult.X, ScaleMult.Y);
		}

		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));

		const float CellSize = GM->GetCellSize();
		const float BaseScale = (ReferenceCellSize > 0.0f) ? (CellSize / ReferenceCellSize) : 1.0f;
		SetActorScale3D(ScaleMult * BaseScale);
	}

	UpdateIntersectionBox();
}

bool ARoadTile::FindMeshConfig(uint8 Mask, UStaticMesh*& OutMesh, float& OutYaw, FVector& OutScaleMultiplier) const
{
	if (Mask == 0)
	{
		return false;
	}

	for (const FRoadMeshConfig& Config : RoadMeshConfigs)
	{
		uint8 Rotated = Config.CanonicalMask;
		for (int32 Rot = 0; Rot < 4; ++Rot)
		{
			if (Rotated == Mask)
			{
				OutMesh = Config.Mesh;
				OutYaw = static_cast<float>(Rot) * 90.0f;
				OutScaleMultiplier = Config.ScaleMultiplier;
				return true;
			}
			Rotated = RotateMask90CW(Rotated);
		}
	}

	return false;
}

uint8 ARoadTile::RotateMask90CW(uint8 Mask)
{
	uint8 Result = 0;
	if (Mask & 0x01) Result |= 0x08;
	if (Mask & 0x08) Result |= 0x02;
	if (Mask & 0x02) Result |= 0x04;
	if (Mask & 0x04) Result |= 0x01;
	return Result;
}

void ARoadTile::OnEnterPlaced_Implementation()
{
	Super::OnEnterPlaced_Implementation();

	UStaticMesh* CurrentMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (CurrentMesh)
	{
		RestoreMeshMaterials(CurrentMesh);
	}
}

void ARoadTile::OnPreviewValidChanged_Implementation(bool bValid)
{
	Super::OnPreviewValidChanged_Implementation(bValid);
}

void ARoadTile::EnsureMeshMaterialsCached(UStaticMesh* Mesh)
{
	if (!Mesh || MeshMaterialCache.Contains(Mesh))
	{
		return;
	}

	const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	Materials.Reserve(StaticMaterials.Num());
	for (const FStaticMaterial& SM : StaticMaterials)
	{
		Materials.Add(SM.MaterialInterface);
	}
	MeshMaterialCache.Add(Mesh, MoveTemp(Materials));
}

void ARoadTile::RestoreMeshMaterials(UStaticMesh* Mesh)
{
	if (!Mesh || !MeshComponent)
	{
		return;
	}

	const TArray<TObjectPtr<UMaterialInterface>>* Cached = MeshMaterialCache.Find(Mesh);
	if (!Cached)
	{
		return;
	}

	for (int32 i = 0; i < Cached->Num(); ++i)
	{
		if ((*Cached)[i])
		{
			MeshComponent->SetMaterial(i, (*Cached)[i]);
		}
	}
}

bool ARoadTile::GetSplinePath(EGridDirection EntryDir, EGridDirection ExitDir, TArray<FVector>& OutWorldPoints) const
{
	OutWorldPoints.Empty();

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	const float HalfCell = GM->GetCellSize() / 2.0f;
	const FTransform& ActorXf = GetActorTransform();

	OutWorldPoints.Add(ActorXf.TransformPosition(GetDirectionLocalVector(EntryDir) * HalfCell));
	OutWorldPoints.Add(ActorXf.TransformPosition(GetDirectionLocalVector(ExitDir) * HalfCell));
	return true;
}

// ================================================================
//  Intersection Box & Lock — Direction-Based Occupancy
// ================================================================

void ARoadTile::UpdateIntersectionBox()
{
	if (!IsPlacedOnGrid())
	{
		IntersectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	const bool bIsIntersection = IsIntersection();

	if (bIsIntersection)
	{
		UGridManager* GM = GetGridManager();
		if (!GM) { return; }

		const float CellSize = GM->GetCellSize();
		const float HalfExtent = CellSize * 0.5f;

		// Cancel inherited actor scale so the world-space box equals exactly one cell.
		const FVector ActorScale = GetActorScale3D();
		const float InvScaleXY = (FMath::Abs(ActorScale.X) > KINDA_SMALL_NUMBER) ? (1.0f / ActorScale.X) : 1.0f;
		const float LocalHalfExtent = HalfExtent * InvScaleXY;
		const float LocalZHalf = IntersectionBoxHalfHeight * InvScaleXY;

		IntersectionBox->SetBoxExtent(FVector(LocalHalfExtent, LocalHalfExtent, LocalZHalf));
		IntersectionBox->SetRelativeLocation(FVector(0.0f, 0.0f, IntersectionBoxHalfHeight * InvScaleXY));
		IntersectionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		UE_LOG(LogRoadTile, Verbose, TEXT("[%s] IntersectionBox ENABLED at grid (%d,%d), cell=%.0f, worldHalf=%.0f, localHalf=%.0f, scale=%.2f"),
			*GetName(), GridPosition.X, GridPosition.Y, CellSize, HalfExtent, LocalHalfExtent, ActorScale.X);
	}
	else
	{
		IntersectionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UpdateIndicator();
}

void ARoadTile::UpdateIndicator()
{
	const bool bIsIntersection = IsIntersection();

	if (!IsPlacedOnGrid() || !bIsIntersection)
	{
		IndicatorPlane->SetVisibility(false);
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM) return;

	const float CellSize = GM->GetCellSize();
	const float ActorScaleXY = GetActorScale3D().X;
	const float InvScale = (FMath::Abs(ActorScaleXY) > KINDA_SMALL_NUMBER) ? (1.0f / ActorScaleXY) : 1.0f;

	const float ZPos = (IntersectionBoxHalfHeight * 2.0f + IndicatorZOffset) * InvScale;
	// Plane mesh is 100x100 world units by default; divide to get correct world-space size
	const float PlaneScale = CellSize * IndicatorSize * InvScale / 100.0f;

	IndicatorPlane->SetRelativeLocation(FVector(0.0f, 0.0f, ZPos));
	IndicatorPlane->SetRelativeScale3D(FVector(PlaneScale, PlaneScale, 1.0f));
	IndicatorPlane->SetVisibility(true);

	if (!IndicatorDMI && IndicatorMaterial)
	{
		IndicatorDMI = UMaterialInstanceDynamic::Create(IndicatorMaterial, this);
		IndicatorPlane->SetMaterial(0, IndicatorDMI);
	}

	UpdateIndicatorState();
}

void ARoadTile::UpdateIndicatorState()
{
	if (!IndicatorDMI) return;

	const bool bOccupied = IsAnyDirectionOccupied();
	IndicatorDMI->SetVectorParameterValue(TEXT("Color"), bOccupied ? IndicatorOccupiedColor : IndicatorFreeColor);
}

bool ARoadTile::IsIntersection() const
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	const FGridCell& Cell = GM->GetCell(GridPosition);
	if (Cell.Type != ECellType::Road)
	{
		return false;
	}

	int32 ConnCount = 0;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Up))    ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Down))  ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Left))  ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;

	return ConnCount >= 3;
}

bool ARoadTile::IsAnyDirectionOccupied() const
{
	// A direction is "occupied" if there's at least one vehicle physically inside
	// or at least one pending reservation for it.
	for (const auto& Pair : DirectionOccupants)
	{
		if (Pair.Value.Num() > 0) return true;
	}
	for (const auto& Pair : PendingReservations)
	{
		if (Pair.Value.Num() > 0) return true;
	}
	return false;
}

// ---- Helper ----

static TSet<EGridDirection> CollectOccupiedDirections(
	const TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>>& DirOccupants,
	const TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>>& PendingRes)
{
	TSet<EGridDirection> Result;
	for (const auto& Pair : DirOccupants)
	{
		if (Pair.Value.Num() > 0) Result.Add(Pair.Key);
	}
	for (const auto& Pair : PendingRes)
	{
		if (Pair.Value.Num() > 0) Result.Add(Pair.Key);
	}
	return Result;
}

static void PurgeInvalidWeakRefs(TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>>& Map)
{
	for (auto& Pair : Map)
	{
		Pair.Value.Remove(TWeakObjectPtr<AVehicleActor>());
	}
	// Remove empty direction keys
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

void ARoadTile::ReleaseVehicleFromAllTables(AVehicleActor* Vehicle)
{
	if (!Vehicle) return;

	// Remove from DirectionOccupants
	for (auto& Pair : DirectionOccupants)
	{
		Pair.Value.Remove(Vehicle);
	}
	// Remove empty keys
	for (auto It = DirectionOccupants.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0) It.RemoveCurrent();
	}

	// Remove from PendingReservations
	for (auto& Pair : PendingReservations)
	{
		Pair.Value.Remove(Vehicle);
	}
	for (auto It = PendingReservations.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0) It.RemoveCurrent();
	}

	// Remove reverse map entry
	VehicleEntryDirs.Remove(Vehicle);

	// Remove timestamp
	PendingReservationTimestamps.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));
}

EGridDirection ARoadTile::FindEntryDirForVehicle(AVehicleActor* Vehicle) const
{
	if (!Vehicle) return EGridDirection::None;
	const EGridDirection* Found = VehicleEntryDirs.Find(TWeakObjectPtr<AVehicleActor>(Vehicle));
	return Found ? *Found : EGridDirection::None;
}

// ---- Public API ----

bool ARoadTile::TryAcquireIntersectionLock(AVehicleActor* Vehicle, EGridDirection EntryDir)
{
	if (!Vehicle || EntryDir == EGridDirection::None)
	{
		return false;
	}

	// 0. Refuse if vehicle has already passed through this intersection.
	//    Prevents self-re-entry: a vehicle that just exited the box can still
	//    sweep it via forward probe and would otherwise re-acquire the lock.
	if (Vehicle->HasPassedIntersection(this))
	{
		return false;
	}

	// 1. Purge stale entries from all tables
	PurgeInvalidWeakRefs(DirectionOccupants);
	PurgeInvalidWeakRefs(PendingReservations);
	// Purge invalid keys from VehicleEntryDirs
	for (auto It = VehicleEntryDirs.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	// 2. Re-entry: vehicle already has any recorded entry in this intersection → always pass.
	//    This eliminates false rejections caused by direction-derivation drift on curved splines
	//    where the probe direction (e.g. Right) diverges from the originally-granted direction (e.g. Down).
	const EGridDirection* ExistingDir = VehicleEntryDirs.Find(TWeakObjectPtr<AVehicleActor>(Vehicle));
	if (ExistingDir)
	{
		UE_LOG(LogRoadTile, Verbose, TEXT("[%s] Lock RE-ENTRY for %s (registered=%s, probing=%s)"),
			*GetName(), *Vehicle->GetName(), *DirToString(*ExistingDir), *DirToString(EntryDir));
		return true;
	}
	// 3. Conflict check with round-robin scheduling.
	//    When multiple directions compete, each direction gets at most
	//    MaxConsecutiveGrants vehicles before the lock switches to the next waiting direction.
	const TSet<EGridDirection> OccupiedDirs = CollectOccupiedDirections(DirectionOccupants, PendingReservations);

	// Helper: reject and enqueue
	auto RejectAndEnqueue = [&](const FString& Reason)
	{
		WaitingDirs.Add(EntryDir);

		FString OccupiedStr;
		for (EGridDirection D : OccupiedDirs) { OccupiedStr += DirToString(D) + TEXT(" "); }

		UE_LOG(LogRoadTile, Verbose, TEXT("[%s] REJECT %s dir=%s (reason=%s, occupied=%s, serving=%s, count=%d)"),
			*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), *Reason, *OccupiedStr,
			*DirToString(ServingDirection), ServedCount);

		if (GEngine && ShouldShowIntersectionScreenDebug())
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("[%s] REJECT %s dir=%s (%s)"),
					*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), *Reason));
		}
	};

	if (OccupiedDirs.Num() == 0)
	{
		// Intersection is physically empty.
		if (WaitingDirs.Num() > 0)
		{
			// ServingDirection was already set by EndOverlap when the last vehicle left.
			// Only grant if the probing direction matches.
			if (EntryDir == ServingDirection)
			{
				ServedCount = 1;
			}
			else
			{
				RejectAndEnqueue(FString::Printf(TEXT("waiting for %s"), *DirToString(ServingDirection)));
				return false;
			}
		}
		else
		{
			// Only one direction (or first arrival): no competition.
			ServingDirection = EntryDir;
			ServedCount = 1;
		}
	}
	else if (OccupiedDirs.Contains(EntryDir))
	{
		// Same direction as current occupants.
		if (ServingDirection == EntryDir && ServedCount < MaxConsecutiveGrants)
		{
			ServedCount++;
		}
		else
		{
			// Turn limit reached. Don't enqueue — this direction already had its
			// turn; enqueuing it would give it a 50% chance of re-winning the
			// next round, starving other directions.
			UE_LOG(LogRoadTile, Verbose, TEXT("[%s] REJECT %s dir=%s (turn limit reached, serving=%s)"),
				*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), *DirToString(ServingDirection));
			return false;
		}
	}
	else
	{
		// Crossing direction — only grant if we're actively serving this direction
		// and haven't exhausted its round yet.
		if (ServingDirection == EntryDir && ServedCount < MaxConsecutiveGrants)
		{
			ServedCount++;
		}
		else
		{
			RejectAndEnqueue(TEXT("crossing"));
			return false;
		}
	}

	// 4. Grant: if this direction was waiting, it's now been served — remove from queue
	WaitingDirs.Remove(EntryDir);

	PendingReservations.FindOrAdd(EntryDir).Add(Vehicle);
	VehicleEntryDirs.Add(TWeakObjectPtr<AVehicleActor>(Vehicle), EntryDir);

	if (UWorld* World = GetWorld())
	{
		PendingReservationTimestamps.Add(TWeakObjectPtr<AVehicleActor>(Vehicle), World->GetTimeSeconds());
	}

	UE_LOG(LogRoadTile, Verbose, TEXT("[%s] Lock GRANTED for %s dir=%s (pending=%d, occupants=%d)"),
		*GetName(), *Vehicle->GetName(), *DirToString(EntryDir),
		PendingReservations.Contains(EntryDir) ? PendingReservations[EntryDir].Num() : 0,
		DirectionOccupants.Contains(EntryDir) ? DirectionOccupants[EntryDir].Num() : 0);

	if (GEngine && ShouldShowIntersectionScreenDebug())
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("[%s] GRANT %s dir=%s (pending=%d)"),
				*GetName(), *Vehicle->GetName(), *DirToString(EntryDir),
				PendingReservations.Contains(EntryDir) ? PendingReservations[EntryDir].Num() : 0));
	}

	return true;
}

// ---- Overlap Events ----

void ARoadTile::OnIntersectionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AVehicleActor* Vehicle = Cast<AVehicleActor>(OtherActor);
	if (!Vehicle)
	{
		return;
	}

	// Find the vehicle's registered entry direction
	EGridDirection EntryDir = FindEntryDirForVehicle(Vehicle);
	if (EntryDir == EGridDirection::None)
	{
		// Vehicle entered the box without a prior reservation — infer direction from velocity
		const FVector Vel = Vehicle->GetVelocityDirection();
		EntryDir = GridDirectionUtils::DirectionFromWorldVector(Vel);
		if (EntryDir == EGridDirection::None)
		{
			UE_LOG(LogRoadTile, Warning, TEXT("[%s] Vehicle %s entered box with unknown direction — skipped"),
				*GetName(), *Vehicle->GetName());
			return;
		}
		VehicleEntryDirs.Add(TWeakObjectPtr<AVehicleActor>(Vehicle), EntryDir);
	}

	// Move from pending → occupants
	{
		TSet<TWeakObjectPtr<AVehicleActor>>* PendingSet = PendingReservations.Find(EntryDir);
		if (PendingSet)
		{
			PendingSet->Remove(Vehicle);
			if (PendingSet->Num() == 0)
			{
				PendingReservations.Remove(EntryDir);
			}
		}
	}

	// Clear timestamp now that vehicle has entered the box
	PendingReservationTimestamps.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));

	DirectionOccupants.FindOrAdd(EntryDir).Add(Vehicle);

	const int32 TotalInBox = DirectionOccupants.Contains(EntryDir) ? DirectionOccupants[EntryDir].Num() : 0;

	UE_LOG(LogRoadTile, Verbose, TEXT("[%s] Vehicle %s ENTERED box dir=%s (occupants: %d)"),
		*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), TotalInBox);

	if (GEngine && ShouldShowIntersectionScreenDebug())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			FString::Printf(TEXT("[%s] ENTER %s dir=%s (inside: %d, pending: %d)"),
				*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), TotalInBox,
				PendingReservations.Contains(EntryDir) ? PendingReservations[EntryDir].Num() : 0));
	}

	UpdateIndicatorState();
}

void ARoadTile::OnIntersectionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AVehicleActor* Vehicle = Cast<AVehicleActor>(OtherActor);
	if (!Vehicle)
	{
		return;
	}

	EGridDirection EntryDir = FindEntryDirForVehicle(Vehicle);
	const bool bHadEntry = (EntryDir != EGridDirection::None);

	// Remove from occupants
	if (bHadEntry)
	{
		TSet<TWeakObjectPtr<AVehicleActor>>* OccSet = DirectionOccupants.Find(EntryDir);
		if (OccSet)
		{
			OccSet->Remove(Vehicle);
			if (OccSet->Num() == 0)
			{
				DirectionOccupants.Remove(EntryDir);
			}
		}
	}

	// Also clean up from pending (defensive, shouldn't normally be needed)
	{
		for (auto& Pair : PendingReservations)
		{
			Pair.Value.Remove(Vehicle);
		}
		for (auto It = PendingReservations.CreateIterator(); It; ++It)
		{
			if (It.Value().Num() == 0) It.RemoveCurrent();
		}
	}

	// Remove from reverse-lookup
	VehicleEntryDirs.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));

	// Only a vehicle that entered with a registered direction actually passed the
	// intersection. This excludes spawn/teleport overlap noise before path setup.
	if (bHadEntry)
	{
		Vehicle->MarkIntersectionPassed(this);
	}

	// Clear timestamp
	PendingReservationTimestamps.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));

	// Purge stale entries
	PurgeInvalidWeakRefs(DirectionOccupants);
	PurgeInvalidWeakRefs(PendingReservations);
	for (auto It = VehicleEntryDirs.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	for (auto It = PendingReservationTimestamps.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	const bool bStillOccupied = IsAnyDirectionOccupied();

	// If the intersection is now completely free, decide who goes next
	if (!bStillOccupied)
	{
		if (WaitingDirs.Num() > 0)
		{
			// Peek the first waiting direction — don't remove yet.
			// Removal happens in TryAcquire when a vehicle from this direction is actually granted.
			EGridDirection NextDir = EGridDirection::None;
			for (EGridDirection D : WaitingDirs)
			{
				NextDir = D;
				break;
			}
			ServingDirection = NextDir;
			ServedCount = 0;

			UE_LOG(LogRoadTile, Verbose, TEXT("[%s] Round-robin: switching serving direction to %s (waiting: %d)"),
				*GetName(), *DirToString(NextDir), WaitingDirs.Num());
		}
		else
		{
			ServingDirection = EGridDirection::None;
			ServedCount = 0;
		}
	}

	UE_LOG(LogRoadTile, Verbose, TEXT("[%s] Vehicle %s EXITED box dir=%s (anyOccluded=%s)"),
		*GetName(), *Vehicle->GetName(), *DirToString(EntryDir), bStillOccupied ? TEXT("YES") : TEXT("no"));

	if (GEngine && ShouldShowIntersectionScreenDebug())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
			FString::Printf(TEXT("[%s] EXIT %s dir=%s (cleared=%s)"),
				*GetName(), *Vehicle->GetName(), *DirToString(EntryDir),
				bStillOccupied ? TEXT("no") : TEXT("YES")));
	}

	UpdateIndicatorState();
}

// ---- Safety nets ----

void ARoadTile::SanitizeOccupants()
{
	PurgeInvalidWeakRefs(DirectionOccupants);
	PurgeInvalidWeakRefs(PendingReservations);

	// For each vehicle still in DirectionOccupants, verify it physically overlaps the box.
	// If EndOverlap was lost (UE edge-case), this is the periodic rescue.
	for (auto OccIt = DirectionOccupants.CreateIterator(); OccIt; ++OccIt)
	{
		TSet<TWeakObjectPtr<AVehicleActor>>& Vehicles = OccIt.Value();
		for (auto VehIt = Vehicles.CreateIterator(); VehIt; ++VehIt)
		{
			AVehicleActor* Vehicle = VehIt->Get();
			if (!Vehicle)
			{
				VehIt.RemoveCurrent();
				continue;
			}

			if (!IntersectionBox->IsOverlappingActor(Vehicle))
			{
				UE_LOG(LogRoadTile, Warning, TEXT("[%s] SANITIZE: %s no longer overlaps box but was still registered as occupant dir=%s — removing"),
					*GetName(), *Vehicle->GetName(), *DirToString(OccIt.Key()));

				VehIt.RemoveCurrent();
				VehicleEntryDirs.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));
				PendingReservationTimestamps.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));
			}
		}
		if (Vehicles.Num() == 0)
		{
			OccIt.RemoveCurrent();
		}
	}

	// Purge stale keys from reverse maps
	for (auto It = VehicleEntryDirs.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	for (auto It = PendingReservationTimestamps.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}

	// If sanitize cleaned everything, decide the next serving direction
	if (!IsAnyDirectionOccupied())
	{
		if (WaitingDirs.Num() > 0)
		{
			// Peek — don't remove. Removal happens in TryAcquire on actual grant.
			EGridDirection NextDir = EGridDirection::None;
			for (EGridDirection D : WaitingDirs)
			{
				NextDir = D;
				break;
			}
			ServingDirection = NextDir;
			ServedCount = 0;
		}
		else
		{
			ServingDirection = EGridDirection::None;
			ServedCount = 0;
		}
	}

	UpdateIndicatorState();
}

void ARoadTile::ExpirePendingReservations(float MaxAgeSeconds)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();

	for (auto& Pair : PendingReservations)
	{
		TSet<TWeakObjectPtr<AVehicleActor>>& Vehicles = Pair.Value;
		for (auto VehIt = Vehicles.CreateIterator(); VehIt; ++VehIt)
		{
			AVehicleActor* Vehicle = VehIt->Get();
			if (!Vehicle)
			{
				VehIt.RemoveCurrent();
				PendingReservationTimestamps.Remove(*VehIt);
				continue;
			}

			const float* Timestamp = PendingReservationTimestamps.Find(TWeakObjectPtr<AVehicleActor>(Vehicle));
			if (Timestamp && (Now - *Timestamp) > MaxAgeSeconds)
			{
				UE_LOG(LogRoadTile, Warning, TEXT("[%s] EXPIRED pending reservation for %s dir=%s (age=%.1fs > max=%.1fs)"),
					*GetName(), *Vehicle->GetName(), *DirToString(Pair.Key), Now - *Timestamp, MaxAgeSeconds);

				VehIt.RemoveCurrent();
				VehicleEntryDirs.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));
				PendingReservationTimestamps.Remove(TWeakObjectPtr<AVehicleActor>(Vehicle));
			}
		}
		if (Vehicles.Num() == 0)
		{
			Pair.Value.Empty(); // mark for removal
		}
	}

	// Remove empty direction keys
	for (auto It = PendingReservations.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0) It.RemoveCurrent();
	}

	UpdateIndicatorState();
}
