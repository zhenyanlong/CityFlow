#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Types/VehicleDataAsset.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Grid/GridManager.h"
#include "Grid/Building.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogVehicleManager, Log, All);

void UVehicleManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UVehicleManager::Deinitialize()
{
	ClearAllVehicles();
	Super::Deinitialize();
}

void UVehicleManager::Tick(float DeltaTime)
{
	if (!bIsActive)
	{
		return;
	}

	TimeSinceLastSpawn += DeltaTime;
	if (TimeSinceLastSpawn >= SpawnInterval)
	{
		TimeSinceLastSpawn = 0.0f;

		TArray<ABuilding*> Origins, Destinations;
		CollectOriginDestinations(Origins, Destinations);

		if (Origins.Num() == 0 || Destinations.Num() == 0)
		{
			UE_LOG(LogVehicleManager, Warning, TEXT("Cannot spawn: Origins=%d, Destinations=%d"),
				Origins.Num(), Destinations.Num());
		}
		else
		{
			TArray<ABuilding*> ShuffledDest = Destinations;
			for (int32 i = ShuffledDest.Num() - 1; i > 0; --i)
			{
				const int32 j = FMath::RandRange(0, i);
				ShuffledDest.Swap(i, j);
			}

			ABuilding* Origin = Origins[FMath::RandRange(0, Origins.Num() - 1)];

			bool bSpawned = false;
			for (ABuilding* Dest : ShuffledDest)
			{
				if (!Dest || Dest == Origin)
				{
					continue;
				}
				if (SpawnVehicle(Origin, Dest))
				{
					bSpawned = true;
					break;
				}
			}

			if (!bSpawned)
			{
				for (ABuilding* OtherOrigin : Origins)
				{
					if (!OtherOrigin || OtherOrigin == Origin)
					{
						continue;
					}
					for (ABuilding* Dest : ShuffledDest)
					{
						if (!Dest || Dest == OtherOrigin)
						{
							continue;
						}
						if (SpawnVehicle(OtherOrigin, Dest))
						{
							bSpawned = true;
							break;
						}
					}
					if (bSpawned) break;
				}
			}

			if (!bSpawned)
			{
				UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: all %d origin × %d destination pairs failed"),
					Origins.Num(), Destinations.Num());
			}
		}
	}

	UpdateIntersectionLocks();

	for (int32 i = ActiveVehicles.Num() - 1; i >= 0; --i)
	{
		AVehicleActor* Vehicle = ActiveVehicles[i];
		if (!Vehicle)
		{
			ActiveVehicles.RemoveAt(i);
			continue;
		}

		if (Vehicle->GetMovementState() == EVehicleMovementState::Arrived)
		{
			ActiveVehicles.RemoveAt(i);
			ArrivedVehicles.Add(Vehicle);
			OnVehicleArrived.Broadcast(Vehicle);
			Vehicle->Destroy();
			continue;
		}

		if (Vehicle->GetMovementState() == EVehicleMovementState::Idle)
		{
			ActiveVehicles.RemoveAt(i);
			Vehicle->Destroy();
			continue;
		}

		UpdateVehicleGridOccupancy(Vehicle);
	}

	if (bIntersectionsDirty)
	{
		CachedIntersections.Reset();
		UGridManager* GM = GetGridManager();
		if (GM)
		{
			for (const FGridVector& RoadCell : GM->GetCellsOfType(ECellType::Road))
			{
				if (IsIntersection(RoadCell))
				{
					CachedIntersections.Add(RoadCell);
				}
			}
		}
		bIntersectionsDirty = false;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (Settings && Settings->bDebugDrawPaths)
	{
		DebugDrawPaths();
	}
	if (Settings && Settings->bDebugDrawIntersections)
	{
		DebugDrawIntersections();
	}
}

void UVehicleManager::StartSpawning()
{
	StopSpawning();

	bIsActive = true;
	bIntersectionsDirty = true;

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (Settings)
	{
		SpawnInterval = Settings->VehicleSpawnInterval;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CongestionTimerHandle,
		this,
		&UVehicleManager::UpdateCongestion,
		CONGESTION_CHECK_INTERVAL,
		true
	);

	TArray<ABuilding*> Origins, Destinations;
	CollectOriginDestinations(Origins, Destinations);

	if (Origins.Num() == 0 || Destinations.Num() == 0)
	{
		return;
	}

	CacheSpawnEntries();

	TimeSinceLastSpawn = SpawnInterval;
}

void UVehicleManager::StopSpawning()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(CongestionTimerHandle);
	}
	bIsActive = false;
}

void UVehicleManager::ClearAllVehicles()
{
	StopSpawning();

	for (AVehicleActor* V : ActiveVehicles)
	{
		if (V)
		{
			V->Destroy();
		}
	}
	ActiveVehicles.Empty();
	ArrivedVehicles.Empty();
	IntersectionLocks.Empty();
	VehicleGridMap.Empty();
	CachedIntersections.Empty();
	CachedSpawnEntries.Empty();
	TotalSpawnWeight = 0.0f;
}

AVehicleActor* UVehicleManager::SpawnVehicle(ABuilding* Origin, ABuilding* Destination)
{
	if (!Origin || !Destination)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: null Origin or Destination"));
		return nullptr;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (Settings && ActiveVehicles.Num() >= Settings->MaxVehicleCount)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: max vehicle count reached (%d/%d)"),
			ActiveVehicles.Num(), Settings->MaxVehicleCount);
		return nullptr;
	}

	TArray<FGridVector> OriginDoorways = Origin->GetDoorwayWorldPositions();

	FGridVector StartPos;
	bool bFoundStart = false;
	for (const FGridVector& Dw : OriginDoorways)
	{
		UGridManager* GM = GetGridManager();
		if (GM && GM->GetCellType(Dw) == ECellType::Road)
		{
			StartPos = Dw;
			bFoundStart = true;
			break;
		}
	}

	if (!bFoundStart)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: Origin %s has no doorway on a Road cell"), *Origin->GetName());
		return nullptr;
	}

	TArray<FGridVector> DestDoorways = Destination->GetDoorwayWorldPositions();

	FGridVector EndPos;
	bool bFoundEnd = false;
	for (const FGridVector& Dw : DestDoorways)
	{
		UGridManager* GM = GetGridManager();
		if (GM && GM->GetCellType(Dw) == ECellType::Road)
		{
			EndPos = Dw;
			bFoundEnd = true;
			break;
		}
	}

	if (!bFoundEnd)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: Destination %s has no doorway on a Road cell"), *Destination->GetName());
		return nullptr;
	}

	TArray<FGridVector> Path;
	if (!BuildPath(StartPos, EndPos, Path))
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: A* path failed from (%d,%d) to (%d,%d)"),
			StartPos.X, StartPos.Y, EndPos.X, EndPos.Y);
		return nullptr;
	}

	FVehicleMovementPlan Plan = BuildMovementPlan(Path);
	if (!Plan.IsValid())
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: movement plan is invalid"));
		return nullptr;
	}

	UGridManager* GM = GetGridManager();
	UWorld* World = GetWorld();
	if (!GM || !World)
	{
		return nullptr;
	}

	const FVector SpawnWorldPos = GM->GridToWorld(StartPos);

	TSubclassOf<AVehicleActor> VehicleClass = PickRandomVehicleClass();
	if (!VehicleClass)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: PickRandomVehicleClass returned null"));
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVehicleActor* Vehicle = World->SpawnActor<AVehicleActor>(
		VehicleClass, SpawnWorldPos, FRotator::ZeroRotator, SpawnParams);

	if (!Vehicle)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: SpawnActor failed for class %s at (%d,%d)"),
			*VehicleClass->GetName(), StartPos.X, StartPos.Y);
		return nullptr;
	}

	Vehicle->Origin = Origin;
	Vehicle->SetDestination(Destination);
	Vehicle->SetMovementPlan(Plan);

	ActiveVehicles.Add(Vehicle);
	OnVehicleSpawned.Broadcast(Vehicle);

	UE_LOG(LogVehicleManager, Log, TEXT("Spawned vehicle %s [%s]: (%d,%d) → (%d,%d), %d waypoints"),
		*Vehicle->GetName(), *VehicleClass->GetName(), StartPos.X, StartPos.Y, EndPos.X, EndPos.Y, Plan.Waypoints.Num());

	return Vehicle;
}

bool UVehicleManager::BuildPath(const FGridVector& Start, const FGridVector& End, TArray<FGridVector>& OutPath) const
{
	TArray<FGridVector> RawPath = FindRoadPath(Start, End);
	if (RawPath.Num() == 0)
	{
		return false;
	}

	OutPath = SmoothPath(RawPath);
	return OutPath.Num() > 0;
}

FVehicleMovementPlan UVehicleManager::BuildMovementPlan(const TArray<FGridVector>& Path) const
{
	FVehicleMovementPlan Plan;
	if (Path.Num() == 0)
	{
		return Plan;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return Plan;
	}

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		const FGridVector& GridPos = Path[i];
		const FVector WorldPos = GM->GridToWorld(GridPos);

		FVehicleWaypoint Waypoint(WorldPos);

		if (IsIntersection(GridPos))
		{
			Waypoint.bIsIntersectionEntry = true;
		}

		if (i > 0 && IsIntersection(Path[i - 1]))
		{
			Waypoint.bIsIntersectionExit = true;
		}

		Plan.Waypoints.Add(Waypoint);
	}

	if (Plan.Waypoints.Num() > 1)
	{
		Plan.Waypoints.Last().bIsIntersectionEntry = false;
	}

	return Plan;
}

TArray<FGridVector> UVehicleManager::FindRoadPath(const FGridVector& Start, const FGridVector& End) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return {};
	}

	if (!GM->IsValidGridPos(Start) || !GM->IsValidGridPos(End))
	{
		return {};
	}

	if (GM->GetCellType(Start) != ECellType::Road || GM->GetCellType(End) != ECellType::Road)
	{
		return {};
	}

	TMap<FGridVector, FAStarNode> OpenMap;
	TMap<FGridVector, FAStarNode> ClosedMap;

	FAStarNode StartNode(Start, 0, ManhattanDist(Start, End), Start);
	OpenMap.Add(Start, StartNode);

	while (OpenMap.Num() > 0)
	{
		FGridVector CurrentPos;
		FAStarNode CurrentNode;
		int32 LowestF = MAX_int32;

		for (const auto& Pair : OpenMap)
		{
			if (Pair.Value.FCost() < LowestF)
			{
				LowestF = Pair.Value.FCost();
				CurrentPos = Pair.Key;
				CurrentNode = Pair.Value;
			}
		}

		OpenMap.Remove(CurrentPos);
		CurrentNode.bClosed = true;
		ClosedMap.Add(CurrentPos, CurrentNode);

		if (CurrentPos == End)
		{
			TArray<FGridVector> Path;
			FGridVector TracePos = End;
			while (TracePos != Start)
			{
				Path.Insert(TracePos, 0);
				const FAStarNode* ParentNode = ClosedMap.Find(TracePos);
				if (!ParentNode)
				{
					return {};
				}
				TracePos = ParentNode->Parent;
				if (Path.Num() > 1000)
				{
					return {};
				}
			}
			Path.Insert(Start, 0);
			return Path;
		}

		const uint8 CurrentMask = GM->GetCell(CurrentPos).ConnectedMask;

		const EGridDirection AllDirs[] = {
			EGridDirection::Up, EGridDirection::Down,
			EGridDirection::Left, EGridDirection::Right
		};

		for (EGridDirection Dir : AllDirs)
		{
			const uint8 DirBit = static_cast<uint8>(Dir);
			if (!(CurrentMask & DirBit))
			{
				continue;
			}

			const FGridVector NeighborPos = CurrentPos + GridDirectionUtils::GetVector(Dir);
			if (!GM->IsValidGridPos(NeighborPos))
			{
				continue;
			}

			const ECellType NeighborType = GM->GetCellType(NeighborPos);
			if (NeighborType != ECellType::Road)
			{
				if (NeighborPos != End)
				{
					continue;
				}
			}

			if (ClosedMap.Contains(NeighborPos))
			{
				continue;
			}

			const int32 NewG = CurrentNode.GCost + 1;

			if (const FAStarNode* ExistingOpen = OpenMap.Find(NeighborPos))
			{
				if (NewG < ExistingOpen->GCost)
				{
					FAStarNode& Node = OpenMap[NeighborPos];
					Node.GCost = NewG;
					Node.Parent = CurrentPos;
				}
			}
			else
			{
				FAStarNode NewNode(NeighborPos, NewG, ManhattanDist(NeighborPos, End), CurrentPos);
				OpenMap.Add(NeighborPos, NewNode);
			}
		}
	}

	return {};
}

TArray<FGridVector> UVehicleManager::SmoothPath(const TArray<FGridVector>& RawPath) const
{
	if (RawPath.Num() <= 2)
	{
		return RawPath;
	}

	TArray<FGridVector> Smoothed;
	Smoothed.Add(RawPath[0]);

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return RawPath;
	}

	for (int32 i = 1; i < RawPath.Num() - 1; ++i)
	{
		const FGridVector& Prev = RawPath[i - 1];
		const FGridVector& Curr = RawPath[i];
		const FGridVector& Next = RawPath[i + 1];

		const bool bDirectionChanged =
			(Curr.X - Prev.X != Next.X - Curr.X) ||
			(Curr.Y - Prev.Y != Next.Y - Curr.Y);

		if (bDirectionChanged)
		{
			Smoothed.Add(Curr);
		}
	}

	Smoothed.Add(RawPath.Last());
	return Smoothed;
}

bool UVehicleManager::CanPathBetween(const FGridVector& A, const FGridVector& B, uint8 MaskA) const
{
	if (B.X == A.X && B.Y == A.Y - 1)
	{
		return (MaskA & static_cast<uint8>(EGridDirection::Up)) != 0;
	}
	if (B.X == A.X && B.Y == A.Y + 1)
	{
		return (MaskA & static_cast<uint8>(EGridDirection::Down)) != 0;
	}
	if (B.X == A.X - 1 && B.Y == A.Y)
	{
		return (MaskA & static_cast<uint8>(EGridDirection::Left)) != 0;
	}
	if (B.X == A.X + 1 && B.Y == A.Y)
	{
		return (MaskA & static_cast<uint8>(EGridDirection::Right)) != 0;
	}
	return false;
}

void UVehicleManager::UpdateCongestion()
{
	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (!Settings)
	{
		return;
	}

	TSet<FGridVector> CongestedSet;
	const int32 Threshold = Settings->CongestionThreshold;

	for (const auto& Pair : VehicleGridMap)
	{
		TArray<AVehicleActor*> VehiclesOnCell;
		for (const auto& P2 : VehicleGridMap)
		{
			if (P2.Key == Pair.Key && P2.Value)
			{
				VehiclesOnCell.Add(P2.Value);
			}
		}

		if (VehiclesOnCell.Num() > Threshold)
		{
			CongestedSet.Add(Pair.Key);

			if (Settings->bDebugDrawCongestion)
			{
				UGridManager* GM = GetGridManager();
				if (GM)
				{
					const FVector WorldPos = GM->GridToWorld(Pair.Key);
					DrawDebugBox(GetWorld(), WorldPos, FVector(50.0f), FColor::Red, false, CONGESTION_CHECK_INTERVAL * 1.5f, 0, 2.0f);
				}
			}
		}
	}

	CongestedCells = CongestedSet;
	OnCongestionUpdated.Broadcast();
}

void UVehicleManager::UpdateIntersectionLocks()
{
	for (auto It = IntersectionLocks.CreateIterator(); It; ++It)
	{
		AVehicleActor* Vehicle = It.Value();
		if (!Vehicle || Vehicle->GetMovementState() == EVehicleMovementState::Arrived)
		{
			It.RemoveCurrent();
			continue;
		}

		if (Vehicle->GetMovementState() != EVehicleMovementState::WaitingIntersection)
		{
			const UGridManager* GM = GetGridManager();
			if (GM)
			{
				const FGridVector VehicleGridPos = GM->WorldToGrid(Vehicle->GetActorLocation());
				if (!(It.Key() == VehicleGridPos))
				{
					It.RemoveCurrent();
				}
			}
		}
	}

	for (AVehicleActor* Vehicle : ActiveVehicles)
	{
		if (!Vehicle)
		{
			continue;
		}

		if (Vehicle->GetMovementState() == EVehicleMovementState::WaitingIntersection)
		{
			const UGridManager* GM = GetGridManager();
			if (!GM)
			{
				continue;
			}

			const FVehicleMovementPlan& Plan = Vehicle->GetMovementPlan();
			const FVehicleWaypoint* CurrentWp = Plan.GetCurrentWaypoint();
			if (!CurrentWp || !CurrentWp->bIsIntersectionEntry)
			{
				continue;
			}

			const FGridVector IntersectionPos = GM->WorldToGrid(CurrentWp->Position);

			AVehicleActor** OccupyingVehicle = IntersectionLocks.Find(IntersectionPos);
			if (!OccupyingVehicle || !(*OccupyingVehicle) || (*OccupyingVehicle) == Vehicle)
			{
				IntersectionLocks.Add(IntersectionPos, Vehicle);
				Vehicle->SetWaitingForIntersection(false);
			}
		}
	}
}

FGridVector UVehicleManager::FindIntersectionEntryPoint(const FGridVector& IntersectionPos, const AVehicleActor* Vehicle) const
{
	return IntersectionPos;
}

bool UVehicleManager::IsIntersection(const FGridVector& Pos) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const FGridCell& Cell = GM->GetCell(Pos);
	if (Cell.Type != ECellType::Road)
	{
		return false;
	}

	int32 ConnCount = 0;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Up)) ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Down)) ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Left)) ++ConnCount;
	if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;

	return ConnCount >= 3;
}

bool UVehicleManager::IsOccupiedByVehicle(const FGridVector& GridPos) const
{
	for (const AVehicleActor* Vehicle : ActiveVehicles)
	{
		if (!Vehicle)
		{
			continue;
		}

		UGridManager* GM = GetGridManager();
		if (GM)
		{
			const FGridVector VehicleGridPos = GM->WorldToGrid(Vehicle->GetActorLocation());
			if (VehicleGridPos == GridPos)
			{
				return true;
			}
		}
	}
	return false;
}

void UVehicleManager::UpdateVehicleGridOccupancy(AVehicleActor* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const FGridVector GridPos = GM->WorldToGrid(Vehicle->GetActorLocation());
	VehicleGridMap.Add(GridPos, Vehicle);
}

void UVehicleManager::CollectOriginDestinations(TArray<ABuilding*>& OutOrigins, TArray<ABuilding*>& OutDestinations) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const TArray<FGridVector> BuildingCells = GM->GetCellsOfType(ECellType::Building);
	TSet<int32> SeenIDs;

	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GM->GetCell(CellPos);
		if (Cell.BuildingID == INDEX_NONE || SeenIDs.Contains(Cell.BuildingID))
		{
			continue;
		}

		SeenIDs.Add(Cell.BuildingID);

		ABuilding* Building = Cast<ABuilding>(Cell.RoadActor);
		if (!Building)
		{
			continue;
		}

		if (Building->bIsDestination)
		{
			OutDestinations.Add(Building);
		}
		else
		{
			OutOrigins.Add(Building);
		}
	}
}

UGridManager* UVehicleManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}

void UVehicleManager::CacheSpawnEntries()
{
	if (CachedSpawnEntries.Num() > 0)
	{
		return;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (!Settings || Settings->DefaultVehicleDataAsset.IsNull())
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("CacheSpawnEntries: DefaultVehicleDataAsset is not configured in Project Settings → CityFlow"));
		return;
	}

	const UVehicleDataAsset* DataAsset = Cast<UVehicleDataAsset>(Settings->DefaultVehicleDataAsset.TryLoad());
	if (!DataAsset)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("CacheSpawnEntries: failed to load DefaultVehicleDataAsset"));
		return;
	}

	CachedSpawnEntries = DataAsset->VehicleEntries;
	TotalSpawnWeight = 0.0f;
	for (const FVehicleSpawnEntry& Entry : CachedSpawnEntries)
	{
		TotalSpawnWeight += FMath::Max(0.0f, Entry.SpawnWeight);
	}

	if (CachedSpawnEntries.Num() == 0)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("CacheSpawnEntries: VehicleEntries array is empty in DataAsset"));
	}
	else
	{
		UE_LOG(LogVehicleManager, Log, TEXT("CacheSpawnEntries: loaded %d vehicle types, total weight=%.1f"),
			CachedSpawnEntries.Num(), TotalSpawnWeight);
	}
}

TSubclassOf<AVehicleActor> UVehicleManager::PickRandomVehicleClass() const
{
	if (CachedSpawnEntries.Num() == 0 || TotalSpawnWeight <= 0.0f)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("PickRandomVehicleClass: no spawn entries configured, falling back to AVehicleActor base class"));
		return AVehicleActor::StaticClass();
	}

	float Roll = FMath::FRand() * TotalSpawnWeight;
	float Accum = 0.0f;

	for (const FVehicleSpawnEntry& Entry : CachedSpawnEntries)
	{
		Accum += FMath::Max(0.0f, Entry.SpawnWeight);
		if (Roll <= Accum && Entry.VehicleClass)
		{
			return Entry.VehicleClass;
		}
	}

	for (int32 i = CachedSpawnEntries.Num() - 1; i >= 0; --i)
	{
		if (CachedSpawnEntries[i].VehicleClass)
		{
			return CachedSpawnEntries[i].VehicleClass;
		}
	}

	return AVehicleActor::StaticClass();
}

int32 UVehicleManager::ManhattanDist(const FGridVector& A, const FGridVector& B)
{
	return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

void UVehicleManager::DebugDrawPaths() const
{
	UGridManager* GM = GetGridManager();
	UWorld* World = GetWorld();
	if (!GM || !World)
	{
		return;
	}

	for (const AVehicleActor* Vehicle : ActiveVehicles)
	{
		if (!Vehicle)
		{
			continue;
		}

		const FVehicleMovementPlan& Plan = Vehicle->GetMovementPlan();
		const FColor PathColor = FColor::MakeFromColorTemperature(
			FMath::Frac(static_cast<float>(Vehicle->GetUniqueID()) * 0.1f) * 1000.0f + 2000.0f);

		for (int32 i = 0; i < Plan.Waypoints.Num() - 1; ++i)
		{
			DrawDebugLine(World, Plan.Waypoints[i].Position + FVector(0, 0, 100),
				Plan.Waypoints[i + 1].Position + FVector(0, 0, 100),
				PathColor, false, 0.1f, 0, 2.0f);
		}

		for (int32 i = 0; i < Plan.Waypoints.Num(); ++i)
		{
			DrawDebugPoint(World, Plan.Waypoints[i].Position + FVector(0, 0, 80),
				15.0f, FColor::Yellow, false, 0.1f);
		}

		DrawDebugSphere(World, Vehicle->GetActorLocation() + FVector(0, 0, 120), 25.0f, 8,
			FColor::Green, false, 0.1f);
	}
}

void UVehicleManager::DebugDrawIntersections() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FGridVector& IntersectionPos : CachedIntersections)
	{
		UGridManager* GM = GetGridManager();
		if (!GM)
		{
			continue;
		}

		const FVector WorldPos = GM->GridToWorld(IntersectionPos);
		const FColor DrawColor = IntersectionLocks.Contains(IntersectionPos) ? FColor::Red : FColor::Orange;

		DrawDebugBox(World, WorldPos + FVector(0, 0, 150), FVector(40, 40, 20),
			DrawColor, false, 0.1f, 0, 2.0f);
	}
}
