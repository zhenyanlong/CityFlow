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

	FGridVector StartPos;
	FGridVector StartBuildingCell;
	bool bFoundStart = false;
	for (const FBuildingDoorway& Dw : Origin->Doorways)
	{
		const FGridVector ConnPt = Origin->GetDoorwayConnectionPoint(Dw);
		UGridManager* GM = GetGridManager();
		if (GM && GM->GetCellType(ConnPt) == ECellType::Road)
		{
			StartPos = ConnPt;
			StartBuildingCell = Origin->GetGridPosition() + Origin->TransformLocalPosition(Dw.RelativePosition);
			bFoundStart = true;
			break;
		}
	}

	if (!bFoundStart)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: Origin %s has no doorway on a Road cell"), *Origin->GetName());
		return nullptr;
	}

	FGridVector EndPos;
	FGridVector EndBuildingCell;
	bool bFoundEnd = false;
	for (const FBuildingDoorway& Dw : Destination->Doorways)
	{
		const FGridVector ConnPt = Destination->GetDoorwayConnectionPoint(Dw);
		UGridManager* GM = GetGridManager();
		if (GM && GM->GetCellType(ConnPt) == ECellType::Road)
		{
			EndPos = ConnPt;
			EndBuildingCell = Destination->GetGridPosition() + Destination->TransformLocalPosition(Dw.RelativePosition);
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

	Path.Insert(StartBuildingCell, 0);
	Path.Add(EndBuildingCell);

	TArray<FVector> SplineTangentDirs;
	TArray<float> SplineArriveLengths;
	TArray<float> SplineLeaveLengths;
	TArray<FVector> SplinePoints = BuildSplinePath(Path, SplineTangentDirs, SplineArriveLengths, SplineLeaveLengths);
	if (SplinePoints.Num() == 0)
	{
		UE_LOG(LogVehicleManager, Warning, TEXT("SpawnVehicle: spline path is empty"));
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

	const float TangentLen = GM->GetCellSize();
	Vehicle->SetSplinePath(SplinePoints, SplineTangentDirs, SplineArriveLengths, SplineLeaveLengths, TangentLen);

	ActiveVehicles.Add(Vehicle);
	OnVehicleSpawned.Broadcast(Vehicle);

	UE_LOG(LogVehicleManager, Log, TEXT("Spawned vehicle %s [%s]: (%d,%d) → (%d,%d), %d spline points"),
		*Vehicle->GetName(), *VehicleClass->GetName(), StartPos.X, StartPos.Y, EndPos.X, EndPos.Y, SplinePoints.Num());

	return Vehicle;
}

bool UVehicleManager::BuildPath(const FGridVector& Start, const FGridVector& End, TArray<FGridVector>& OutPath) const
{
	OutPath = FindRoadPath(Start, End);
	return OutPath.Num() > 0;
}

static FVector GridDeltaToWorldDir(const FGridVector& Delta)
{
	return FVector(static_cast<float>(Delta.X), static_cast<float>(Delta.Y), 0.0f).GetSafeNormal();
}

TArray<FVector> UVehicleManager::BuildSplinePath(const TArray<FGridVector>& Path,
	TArray<FVector>& OutTangentDirs,
	TArray<float>& OutArriveTangentLengths,
	TArray<float>& OutLeaveTangentLengths) const
{
	TArray<FVector> AllPoints;
	OutTangentDirs.Reset();
	OutArriveTangentLengths.Reset();
	OutLeaveTangentLengths.Reset();
	if (Path.Num() == 0)
	{
		return AllPoints;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return AllPoints;
	}

	const float HalfCell = GM->GetCellSize() * 0.5f;
	const float TurnLongMult  = 1.0f + LaneOffsetFactor;
	const float TurnShortMult = 1.0f - LaneOffsetFactor;
	const bool bRightHand = (DrivingSide == ECityFlowDrivingSide::RightHand);

	// Helper to add a point with given arrive/leave length multipliers
	auto AddPoint = [&](const FVector& Pos, const FVector& TangentDir, float ArriveLen, float LeaveLen)
	{
		AllPoints.Add(Pos);
		OutTangentDirs.Add(TangentDir);
		OutArriveTangentLengths.Add(ArriveLen);
		OutLeaveTangentLengths.Add(LeaveLen);
	};

	if (Path.Num() == 1)
	{
		AddPoint(GM->GridToWorld(Path[0]), FVector::ForwardVector, 1.0f, 1.0f);
		return AllPoints;
	}

	const FVector StartTangentDir = GridDeltaToWorldDir(Path[1] - Path[0]);
	AddPoint(GM->GridToWorld(Path[0]), StartTangentDir, 1.0f, 1.0f);

	bool bPreviousWasTurn = false;

	for (int32 i = 1; i < Path.Num() - 1; ++i)
	{
		const FGridVector& Prev = Path[i - 1];
		const FGridVector& Curr = Path[i];
		const FGridVector& Next = Path[i + 1];

		const FGridVector EntryDelta = Curr - Prev;
		const FGridVector ExitDelta = Next - Curr;

		const bool bIsTurn = (EntryDelta.X != ExitDelta.X) || (EntryDelta.Y != ExitDelta.Y);

		if (bIsTurn)
		{
			const FVector Center = GM->GridToWorld(Curr);
			const FVector EntryDir = GridDeltaToWorldDir(EntryDelta);
			const FVector ExitDir = GridDeltaToWorldDir(ExitDelta);

			const float CrossZ = EntryDir.X * ExitDir.Y - EntryDir.Y * ExitDir.X;
			const bool bIsRightTurn = CrossZ > 0.0f;
			const bool bOutside = (bRightHand == bIsRightTurn);
			const float TurnMult = bOutside ? TurnLongMult : TurnShortMult;

			if (bPreviousWasTurn)
			{
				// Consecutive turn: previous turn's exit point is treated as
				// this turn's entry — update that point's leave tangent to this TurnMult
				const int32 LastIdx = OutLeaveTangentLengths.Num() - 1;
				OutLeaveTangentLengths[LastIdx] = TurnMult;
			}
			else
			{
				// First turn in sequence: add entry offset
				//       arrive = 1.0  (from previous straight)
				//       leave  = TurnMult  (face turn curve)
				AddPoint(Center - EntryDir * HalfCell, EntryDir, 1.0f, TurnMult);
			}

			// Add exit offset
			//       arrive = TurnMult  (face turn curve)
			//       leave  = 1.0  (default — may be overridden if next is consecutive)
			AddPoint(Center + ExitDir * HalfCell, ExitDir, TurnMult, 1.0f);
		}
		else
		{
			// Straight cell: both arrive and leave = 1.0 (resets turn sequence)
			AddPoint(GM->GridToWorld(Curr), GridDeltaToWorldDir(ExitDelta), 1.0f, 1.0f);
		}

		bPreviousWasTurn = bIsTurn;
	}

	const FVector EndTangentDir = GridDeltaToWorldDir(Path.Last() - Path[Path.Num() - 2]);
	AddPoint(GM->GridToWorld(Path.Last()), EndTangentDir, 1.0f, 1.0f);

	// Apply bidirectional lane offset
	if (LaneOffsetFactor > 0.0f)
	{
		const float LaneOffset = GM->GetCellSize() * LaneOffsetFactor;

		for (int32 i = 0; i < AllPoints.Num(); ++i)
		{
			const FVector Tangent = OutTangentDirs[i].GetSafeNormal();
			const FVector RightPerp(Tangent.Y, -Tangent.X, 0.0f);

			if (bRightHand)
			{
				AllPoints[i] += RightPerp * LaneOffset;
			}
			else
			{
				AllPoints[i] -= RightPerp * LaneOffset;
			}
		}
	}

	return AllPoints;
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
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	for (auto It = IntersectionLocks.CreateIterator(); It; ++It)
	{
		TMap<TObjectPtr<AVehicleActor>, EGridDirection>& VehiclesAtIntersection = It.Value();

		for (auto VehIt = VehiclesAtIntersection.CreateIterator(); VehIt; ++VehIt)
		{
			AVehicleActor* Vehicle = VehIt.Key();
			if (!Vehicle || !ActiveVehicles.Contains(Vehicle) ||
				Vehicle->GetMovementState() == EVehicleMovementState::Arrived)
			{
				VehIt.RemoveCurrent();
				continue;
			}

			const FGridVector VehicleGrid = GM->WorldToGrid(Vehicle->GetActorLocation());
			if (VehicleGrid != It.Key())
			{
				VehIt.RemoveCurrent();
			}
		}

		if (VehiclesAtIntersection.Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

bool UVehicleManager::IsIntersectionLockedByOther(const FGridVector& Pos, const AVehicleActor* AskingVehicle, EGridDirection EntryDir) const
{
	const TMap<TObjectPtr<AVehicleActor>, EGridDirection>* VehiclesAtPos = IntersectionLocks.Find(Pos);
	if (!VehiclesAtPos || VehiclesAtPos->Num() == 0)
	{
		return false;
	}

	for (const auto& Pair : *VehiclesAtPos)
	{
		const AVehicleActor* ExistingVehicle = Pair.Key;
		const EGridDirection ExistingDir = Pair.Value;

		if (!ExistingVehicle || ExistingVehicle == AskingVehicle)
		{
			continue;
		}

		// Only block if the entry directions are perpendicular (crossing paths)
		// Same direction: same lane, no conflict
		// Opposite direction: opposing lanes, no conflict
		if (GridDirectionUtils::ArePerpendicular(ExistingDir, EntryDir))
		{
			return true;
		}
	}

	return false;
}

void UVehicleManager::AcquireIntersectionLock(const FGridVector& Pos, AVehicleActor* Vehicle, EGridDirection EntryDir)
{
	if (!Vehicle || EntryDir == EGridDirection::None)
	{
		return;
	}

	TMap<TObjectPtr<AVehicleActor>, EGridDirection>& VehiclesAtPos = IntersectionLocks.FindOrAdd(Pos);
	VehiclesAtPos.Add(Vehicle, EntryDir);
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

		const USplineComponent* Spline = Vehicle->GetPathSpline();
		if (!Spline || Spline->GetNumberOfSplinePoints() < 2)
		{
			continue;
		}

		const FColor PathColor = FColor::MakeFromColorTemperature(
			FMath::Frac(static_cast<float>(Vehicle->GetUniqueID()) * 0.1f) * 1000.0f + 2000.0f);

		const float TotalLength = Spline->GetSplineLength();
		const int32 NumSegments = FMath::CeilToInt32(TotalLength / 60.0f);
		FVector PrevPos = Spline->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);

		for (int32 i = 1; i <= NumSegments; ++i)
		{
			const float Dist = (static_cast<float>(i) / NumSegments) * TotalLength;
			const FVector CurrPos = Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
			DrawDebugLine(World, PrevPos + FVector(0, 0, 100), CurrPos + FVector(0, 0, 100),
				PathColor, false, 0.1f, 0, 2.0f);
			PrevPos = CurrPos;
		}

		for (int32 i = 0; i <= NumSegments; ++i)
		{
			const float Dist = (static_cast<float>(i) / NumSegments) * TotalLength;
			const FVector Pt = Spline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
			DrawDebugPoint(World, Pt + FVector(0, 0, 80), 12.0f, FColor::Yellow, false, 0.1f);
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
		const TMap<TObjectPtr<AVehicleActor>, EGridDirection>* VehiclesAtPos = IntersectionLocks.Find(IntersectionPos);
		const bool bLocked = VehiclesAtPos && VehiclesAtPos->Num() > 0;
		const FColor DrawColor = bLocked ? FColor::Red : FColor::Orange;

		DrawDebugBox(World, WorldPos + FVector(0, 0, 150), FVector(40, 40, 20),
			DrawColor, false, 0.1f, 0, 2.0f);
	}
}
