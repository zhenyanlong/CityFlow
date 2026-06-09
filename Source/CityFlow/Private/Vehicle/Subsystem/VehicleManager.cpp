#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Types/VehicleDataAsset.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Grid/GridManager.h"
#include "Grid/Building.h"
#include "Grid/RoadTile.h"
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

		// Filter out origins blocked by a vehicle that hasn't left the building cell
		TArray<ABuilding*> UnblockedOrigins;
		UnblockedOrigins.Reserve(Origins.Num());
		for (ABuilding* OriginBld : Origins)
		{
			if (!IsBuildingBlocked(OriginBld))
			{
				UnblockedOrigins.Add(OriginBld);
			}
		}

		if (UnblockedOrigins.Num() == 0 || Destinations.Num() == 0)
		{
			UE_LOG(LogVehicleManager, Warning, TEXT("Cannot spawn: UnblockedOrigins=%d, Destinations=%d"),
				UnblockedOrigins.Num(), Destinations.Num());
		}
		else
		{
			TArray<ABuilding*> ShuffledDest = Destinations;
			for (int32 i = ShuffledDest.Num() - 1; i > 0; --i)
			{
				const int32 j = FMath::RandRange(0, i);
				ShuffledDest.Swap(i, j);
			}

			ABuilding* Origin = UnblockedOrigins[FMath::RandRange(0, UnblockedOrigins.Num() - 1)];

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
				for (ABuilding* OtherOrigin : UnblockedOrigins)
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
					UnblockedOrigins.Num(), Destinations.Num());
			}
		}
	}

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

	// Periodic intersection lock sanitization — cleans up lost EndOverlap events and expired pending reservations
	World->GetTimerManager().SetTimer(
		SanitizeTimerHandle,
		this,
		&UVehicleManager::SanitizeAllIntersectionLocks,
		2.0f,
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
		World->GetTimerManager().ClearTimer(SanitizeTimerHandle);
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
				const int32 LastIdx = OutLeaveTangentLengths.Num() - 1;
				OutLeaveTangentLengths[LastIdx] = TurnMult;
			}
			else
			{
				AddPoint(Center - EntryDir * HalfCell, EntryDir, 1.0f, TurnMult);
			}

			AddPoint(Center + ExitDir * HalfCell, ExitDir, TurnMult, 1.0f);
		}
		else
		{
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

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	// Build per-cell vehicle count from active vehicles each tick
	TMap<FGridVector, int32> CellCounts;
	for (const AVehicleActor* Vehicle : ActiveVehicles)
	{
		if (!Vehicle || Vehicle->GetMovementState() == EVehicleMovementState::Arrived)
		{
			continue;
		}
		const FGridVector GridPos = GM->WorldToGrid(Vehicle->GetActorLocation());
		++CellCounts.FindOrAdd(GridPos, 0);
	}

	TSet<FGridVector> CongestedSet;
	const int32 Threshold = Settings->CongestionThreshold;

	for (const auto& Pair : CellCounts)
	{
		if (Pair.Value > Threshold)
		{
			CongestedSet.Add(Pair.Key);

			if (Settings->bDebugDrawCongestion)
			{
				const FVector WorldPos = GM->GridToWorld(Pair.Key);
				DrawDebugBox(GetWorld(), WorldPos, FVector(50.0f), FColor::Red, false, CONGESTION_CHECK_INTERVAL * 1.5f, 0, 2.0f);
			}
		}
	}

	CongestedCells = CongestedSet;
	OnCongestionUpdated.Broadcast();
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

bool UVehicleManager::IsBuildingBlocked(ABuilding* Building) const
{
	if (!Building)
	{
		return false;
	}

	const FGridVector BuildingAnchor = Building->GetGridPosition();
	const FVector2D BSize = Building->GetEffectiveBuildingSize();
	const int32 SizeX = FMath::Max(1, FMath::RoundToInt(BSize.X));
	const int32 SizeY = FMath::Max(1, FMath::RoundToInt(BSize.Y));

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	for (const AVehicleActor* Vehicle : ActiveVehicles)
	{
		if (!Vehicle || Vehicle->Origin != Building)
		{
			continue;
		}

		const FGridVector VehicleGrid = GM->WorldToGrid(Vehicle->GetActorLocation());
		const FGridVector LocalPos = VehicleGrid - BuildingAnchor;

		if (LocalPos.X >= 0 && LocalPos.X < SizeX &&
			LocalPos.Y >= 0 && LocalPos.Y < SizeY)
		{
			return true;
		}
	}

	return false;
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

	// 优先使用 GameMode 传入的 DataAsset
	if (ExternalVehicleDataAsset)
	{
		CachedSpawnEntries = ExternalVehicleDataAsset->VehicleEntries;
	}
	else
	{
		// 回退到 DeveloperSettings
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
	}

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
	UGridManager* GM = GetGridManager();
	if (!World || !GM)
	{
		return;
	}

	// Iterate all ARoadTile actors in the world and query their lock state
	for (TActorIterator<ARoadTile> It(World); It; ++It)
	{
		ARoadTile* Tile = *It;
		if (!Tile || !Tile->IsIntersection())
		{
			continue;
		}

		const bool bLocked = Tile->IsAnyDirectionOccupied();
		const FColor DrawColor = bLocked ? FColor::Red : FColor::Orange;
		const FVector WorldPos = Tile->GetActorLocation();

		DrawDebugBox(World, WorldPos + FVector(0, 0, 150), FVector(40, 40, 20),
			DrawColor, false, 0.1f, 0, 2.0f);
	}
}

void UVehicleManager::SanitizeAllIntersectionLocks()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ARoadTile> It(World); It; ++It)
	{
		ARoadTile* Tile = *It;
		if (!Tile || !Tile->IsIntersection()) continue;

		Tile->SanitizeOccupants();
		Tile->ExpirePendingReservations(5.0f);
	}
}
