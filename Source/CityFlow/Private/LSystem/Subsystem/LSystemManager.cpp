#include "LSystem/Subsystem/LSystemManager.h"
#include "Grid/GridManager.h"
#include "Grid/RoadTile.h"
#include "Grid/Building.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowLSystem, Log, All);

void ULSystemManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void ULSystemManager::Deinitialize()
{
	if (bIsGenerating)
	{
		GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
		PendingGrowthPoints.Empty();
		bIsGenerating = false;
	}

	Super::Deinitialize();
}

UGridManager* ULSystemManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}

void ULSystemManager::SetRoadTileClass(TSubclassOf<ARoadTile> InClass)
{
	RoadTileClass = InClass;
}

void ULSystemManager::SetBranchBudget(int32 NewBudget)
{
	BranchBudget = FMath::Max(0, NewBudget);
}

void ULSystemManager::SetGrowthInterval(float NewInterval)
{
	GrowthInterval = FMath::Max(0.01f, NewInterval);
}

void ULSystemManager::SetBranchProbability(float NewProbability)
{
	BranchProbability = FMath::Clamp(NewProbability, 0.0f, 1.0f);
}

void ULSystemManager::SetAttractionStrength(float NewStrength)
{
	AttractionStrength = FMath::Clamp(NewStrength, 0.0f, 1.0f);
}

void ULSystemManager::SetStraightExtendLength(int32 NewLength)
{
	StraightExtendLength = FMath::Max(1, NewLength);
}

void ULSystemManager::SetMinBranchSpacing(int32 NewSpacing)
{
	MinBranchSpacing = FMath::Max(1, NewSpacing);
}

void ULSystemManager::AddBudget(int32 Amount)
{
	BranchBudget = FMath::Max(0, BranchBudget + Amount);
	RemainingBudget = GetGenerationBudgetRemaining();
}

void ULSystemManager::StartGenerate()
{
	// Planning is performed before the timer starts. This prevents early visual
	// growth from spending tiles that a later building connection still requires.
	if (bIsGenerating)
	{
		AbortGeneration();
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	if (!RoadTileClass)
	{
		return;
	}

	CellsPlacedThisGeneration = 0;
	RemainingBudget = FMath::Min(BranchBudget, GM->GetRemainingBudget());
	GenerationRandom.Initialize(FMath::Rand());
	bIsGenerating = true;

	PendingGrowthPoints.Empty();
	QueuedGrowthPoints.Empty();
	PendingConnectionCells.Empty();
	NextConnectionCellIndex = 0;
	CollectStartPoints();
	BuildConnectionPlan();
	UE_LOG(LogCityFlowLSystem, Log,
		TEXT("Generation started: branch budget=%d, grid budget=%d, organic frontiers=%d, reserved connection cells=%d"),
		BranchBudget, GM->GetRemainingBudget(), PendingGrowthPoints.Num(), CountPendingConnectionCost());

	OnGenerationStarted.Broadcast();

	if ((PendingGrowthPoints.Num() == 0 && CountPendingConnectionCost() == 0) || RemainingBudget <= 0)
	{
		FinishGeneration(AreAllBuildingsConnected());
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		GrowthTimerHandle,
		this,
		&ULSystemManager::ProcessGrowthStep,
		GrowthInterval,
		false
	);
}

void ULSystemManager::AbortGeneration()
{
	if (!bIsGenerating)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
	PendingGrowthPoints.Empty();
	QueuedGrowthPoints.Empty();
	PendingConnectionCells.Empty();
	NextConnectionCellIndex = 0;

	const bool bAllConnected = AreAllBuildingsConnected();
	bIsGenerating = false;
	OnGenerationFinished.Broadcast(bAllConnected);
}

void ULSystemManager::CollectStartPoints()
{
	CollectStartPointsFromRoads();
	CollectStartPointsFromBuildings();
}

void ULSystemManager::EnqueueGrowthPoint(const FLSystemGrowthPoint& Point)
{
	if (Point.Direction == EGridDirection::None || QueuedGrowthPoints.Contains(Point))
	{
		return;
	}

	QueuedGrowthPoints.Add(Point);
	PendingGrowthPoints.Add(Point);
}

void ULSystemManager::CollectStartPointsFromRoads()
{
	// Existing player roads are treated as immutable anchors. Dead ends continue
	// forward, while sufficiently spaced straight segments may expose side-growth
	// candidates without changing the topology the player already authored.
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const TArray<FGridVector> RoadCells = GM->GetCellsOfType(ECellType::Road);
	const EGridDirection AllDirs[] = {
		EGridDirection::Up, EGridDirection::Down,
		EGridDirection::Left, EGridDirection::Right
	};

	TSet<FGridVector> Visited;

	for (const FGridVector& RoadPos : RoadCells)
	{
		if (Visited.Contains(RoadPos))
		{
			continue;
		}

		const FGridCell& Cell = GM->GetCell(RoadPos);
		const uint8 Mask = Cell.ConnectedMask;
		const int32 NumConns = PopCount(Mask);

		if (IsDeadEnd(Mask))
		{
			Visited.Add(RoadPos);
			// Continue away from the existing road. Side branches are generated later
			// by TryGrowAt and remain controlled by BranchProbability.
			for (EGridDirection Dir : AllDirs)
			{
				const uint8 DirBit = static_cast<uint8>(Dir);
				if (!(Mask & DirBit))
				{
					const EGridDirection ForwardDir = OppositeDirection(Dir);
					if (CanGrowInDirection(RoadPos, ForwardDir))
					{
						EnqueueGrowthPoint(FLSystemGrowthPoint(RoadPos, ForwardDir));
					}
					break;
				}
			}
		}
		else if (IsStraightRoad(Mask))
		{
			const EGridDirection AxisDirA = (Mask & static_cast<uint8>(EGridDirection::Up)) || (Mask & static_cast<uint8>(EGridDirection::Down))
				? EGridDirection::Up : EGridDirection::Left;
			const EGridDirection AxisDirB = OppositeDirection(AxisDirA);

			TArray<FGridVector> SegmentCells;
			SegmentCells.Add(RoadPos);

			FGridVector Walker = RoadPos + GridDirectionUtils::GetVector(AxisDirA);
			while (GM->IsValidGridPos(Walker)
				&& GM->GetCellType(Walker) == ECellType::Road
				&& IsStraightRoad(GM->GetCell(Walker).ConnectedMask)
				&& GM->GetCell(Walker).ConnectedMask == Mask)
			{
				SegmentCells.Insert(Walker, 0);
				Walker = Walker + GridDirectionUtils::GetVector(AxisDirA);
			}

			Walker = RoadPos + GridDirectionUtils::GetVector(AxisDirB);
			while (GM->IsValidGridPos(Walker)
				&& GM->GetCellType(Walker) == ECellType::Road
				&& IsStraightRoad(GM->GetCell(Walker).ConnectedMask)
				&& GM->GetCell(Walker).ConnectedMask == Mask)
			{
				SegmentCells.Add(Walker);
				Walker = Walker + GridDirectionUtils::GetVector(AxisDirB);
			}

			for (const FGridVector& SegCell : SegmentCells)
			{
				Visited.Add(SegCell);
			}

			const int32 SegLen = SegmentCells.Num();
			if (SegLen < 3)
			{
				continue;
			}

			const EGridDirection PerpA = TurnLeft(AxisDirA);
			const EGridDirection PerpB = TurnRight(AxisDirA);

			const int32 LastIdx = SegLen - 2;

			for (int32 i = 1; i <= LastIdx; i += MinBranchSpacing + 1)
			{
				const FGridVector& SamplePos = SegmentCells[i];

				if (GM->IsValidGridPos(SamplePos + GridDirectionUtils::GetVector(PerpA))
					&& GM->GetCellType(SamplePos + GridDirectionUtils::GetVector(PerpA)) == ECellType::Empty)
				{
					EnqueueGrowthPoint(FLSystemGrowthPoint(SamplePos, PerpA));
				}

				if (GM->IsValidGridPos(SamplePos + GridDirectionUtils::GetVector(PerpB))
					&& GM->GetCellType(SamplePos + GridDirectionUtils::GetVector(PerpB)) == ECellType::Empty)
				{
					EnqueueGrowthPoint(FLSystemGrowthPoint(SamplePos, PerpB));
				}
			}
		}
		else
		{
			Visited.Add(RoadPos);
		}
	}
}

void ULSystemManager::CollectStartPointsFromBuildings()
{
	// Doorways are the only legal interface between a building footprint and the
	// road graph. Using the transformed doorway direction is essential for rotated
	// rectangular buildings; the actor origin alone is not a valid connection cell.
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	TArray<ABuilding*> Buildings = GetAllBuildings();
	const TSet<FGridVector> PrimaryComponent = GetPrimaryRoadComponent();
	for (ABuilding* Building : Buildings)
	{
		if (!Building || IsBuildingConnected(Building))
		{
			continue;
		}

		bool bFoundDoorway = false;
		int32 BestDistance = MAX_int32;
		FGridVector BestBasePos;
		FGridVector BestConnectionPoint;

		for (const FBuildingDoorway& Doorway : Building->Doorways)
		{
			const FGridVector ConnPt = Building->GetDoorwayConnectionPoint(Doorway);
			if (!GM->IsValidGridPos(ConnPt))
			{
				continue;
			}

			if (GM->GetCellType(ConnPt) != ECellType::Empty)
			{
				continue;
			}

			const FGridVector LocalPos = Building->TransformLocalPosition(Doorway.RelativePosition);
			const FGridVector BasePos = Building->GetGridPosition() + LocalPos;

			int32 Distance = 0;
			if (!PrimaryComponent.IsEmpty())
			{
				Distance = MAX_int32;
				for (const FGridVector& RoadPos : PrimaryComponent)
				{
					Distance = FMath::Min(Distance,
						FMath::Abs(ConnPt.X - RoadPos.X) + FMath::Abs(ConnPt.Y - RoadPos.Y));
				}
			}

			if (!bFoundDoorway || Distance < BestDistance)
			{
				bFoundDoorway = true;
				BestDistance = Distance;
				BestBasePos = BasePos;
				BestConnectionPoint = ConnPt;
			}
		}

		if (bFoundDoorway)
		{
			const EGridDirection RotatedDirection =
				GridDirectionUtils::DirectionFromGridDelta(BestConnectionPoint - BestBasePos);
			EnqueueGrowthPoint(FLSystemGrowthPoint(BestBasePos, RotatedDirection));
		}
	}
}

void ULSystemManager::ProcessGrowthStep()
{
	// Commit at most one visible unit of work per timer tick. Besides producing a
	// readable growth animation, this keeps delegates and neighbour-mask updates in
	// a deterministic order that UI and tests can observe.
	if (!bIsGenerating)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		FinishGeneration(false);
		return;
	}

	RemainingBudget = GetGenerationBudgetRemaining();
	if (AreAllBuildingsConnected())
	{
		FinishGeneration(true);
		return;
	}
	if (RemainingBudget <= 0)
	{
		FinishGeneration(false);
		return;
	}

	bool bDidWork = false;
	const int32 ReservedConnectionCost = CountPendingConnectionCost();

	// Organic growth can only spend budget that is not reserved for the
	// connectivity plan. Globally prioritise frontier points by attraction.
	if (PendingGrowthPoints.Num() > 0 && RemainingBudget > ReservedConnectionCost)
	{
		PendingGrowthPoints.Sort([this](const FLSystemGrowthPoint& A, const FLSystemGrowthPoint& B)
		{
			ABuilding* TargetA = FindNearestUnconnectedBuilding(A.Position);
			ABuilding* TargetB = FindNearestUnconnectedBuilding(B.Position);
			return GetAttractionScore(A.Position, TargetA, A.Direction)
				< GetAttractionScore(B.Position, TargetB, B.Direction);
		});

		const FLSystemGrowthPoint Point = PendingGrowthPoints.Pop(EAllowShrinking::No);
		QueuedGrowthPoints.Remove(Point);
		const bool bPlacedGrowth = TryGrowAt(Point);
		// A blocked frontier is not a terminal condition while other candidates remain.
		bDidWork = bPlacedGrowth || PendingGrowthPoints.Num() > 0;
	}

	if (!bDidWork && CountPendingConnectionCost() > 0)
	{
		bDidWork = ProcessConnectionPlanStep();
	}

	if (!bDidWork && PendingGrowthPoints.Num() == 0)
	{
		BuildConnectionPlan();
		if (CountPendingConnectionCost() > 0)
		{
			bDidWork = ProcessConnectionPlanStep();
		}
	}

	RemainingBudget = GetGenerationBudgetRemaining();
	OnGenerationStep.Broadcast(RemainingBudget);

	if (AreAllBuildingsConnected())
	{
		FinishGeneration(true);
		return;
	}
	if (!bDidWork || RemainingBudget <= 0
		|| (PendingGrowthPoints.Num() == 0 && CountPendingConnectionCost() == 0))
	{
		FinishGeneration(false);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		GrowthTimerHandle,
		this,
		&ULSystemManager::ProcessGrowthStep,
		GrowthInterval,
		false
	);
}

bool ULSystemManager::ProcessConnectionPlanStep()
{
	// Connection paths were costed before growth began. Existing road cells have
	// zero placement cost, so replaying the plan may traverse them without spending
	// budget; only newly occupied cells reduce the shared pool.
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	while (NextConnectionCellIndex < PendingConnectionCells.Num())
	{
		const FGridVector GridPos = PendingConnectionCells[NextConnectionCellIndex++];
		const ECellType CellType = GM->GetCellType(GridPos);
		if (CellType == ECellType::Road)
		{
			continue;
		}
		if (CellType != ECellType::Empty || GetGenerationBudgetRemaining() <= 0)
		{
			return false;
		}

		if (CreateRoadTile(GridPos))
		{
			++CellsPlacedThisGeneration;
			return true;
		}
		return false;
	}

	return false;
}

bool ULSystemManager::TryGrowAt(const FLSystemGrowthPoint& Point)
{
	// A growth point is directional state, not just a cell. Keeping direction in the
	// visited key prevents accidental infinite loops while still allowing the same
	// junction to be considered from another meaningful approach.
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	FGridVector LastPlacedPos = Point.Position;
	int32 CellsPlaced = 0;

	for (int32 Step = 0; Step < StraightExtendLength; ++Step)
	{
		const FGridVector NextPos = LastPlacedPos + GridDirectionUtils::GetVector(Point.Direction);

		if (!GM->IsValidGridPos(NextPos) || GM->GetCellType(NextPos) != ECellType::Empty)
		{
			break;
		}

		if (GetGenerationBudgetRemaining() <= CountPendingConnectionCost())
		{
			break;
		}

		ARoadTile* Tile = CreateRoadTile(NextPos);
		if (!Tile)
		{
			break;
		}

		LastPlacedPos = NextPos;
		++CellsPlaced;
		++CellsPlacedThisGeneration;
	}

	if (CellsPlaced == 0)
	{
		return false;
	}

	ABuilding* Target = FindNearestUnconnectedBuilding(LastPlacedPos);

	const uint8 NewMask = GM->CalculateConnectedMask(LastPlacedPos);

	const EGridDirection AllDirs[] = {
		EGridDirection::Up, EGridDirection::Down,
		EGridDirection::Left, EGridDirection::Right
	};

	TArray<FLSystemGrowthPoint> NewPoints;

	const EGridDirection BackDir = OppositeDirection(Point.Direction);

	for (EGridDirection Dir : AllDirs)
	{
		if (Dir == BackDir)
		{
			continue;
		}

		const uint8 DirBit = static_cast<uint8>(Dir);
		if (NewMask & DirBit)
		{
			continue;
		}

		const FGridVector NeighborPos = LastPlacedPos + GridDirectionUtils::GetVector(Dir);
		if (!GM->IsValidGridPos(NeighborPos) || GM->GetCellType(NeighborPos) != ECellType::Empty)
		{
			continue;
		}

		if (Dir == Point.Direction)
		{
			NewPoints.Add(FLSystemGrowthPoint(LastPlacedPos, Dir));
		}
		else
		{
			if (!IsSideBranchValid(LastPlacedPos, Dir))
			{
				continue;
			}

			if (GenerationRandom.FRand() < BranchProbability)
			{
				NewPoints.Add(FLSystemGrowthPoint(LastPlacedPos, Dir));
			}
		}
	}

	if (Target && NewPoints.Num() > 0)
	{
		NewPoints.Sort([this, &Target](const FLSystemGrowthPoint& A, const FLSystemGrowthPoint& B)
		{
			return GetAttractionScore(A.Position, Target, A.Direction)
				> GetAttractionScore(B.Position, Target, B.Direction);
		});
	}

	for (const FLSystemGrowthPoint& NewPt : NewPoints)
	{
		EnqueueGrowthPoint(NewPt);
	}

	return true;
}

ARoadTile* ULSystemManager::CreateRoadTile(const FGridVector& GridPos)
{
	UGridManager* GM = GetGridManager();
	UWorld* World = GetWorld();
	if (!GM || !World)
	{
		return nullptr;
	}

	const FVector WorldPos = GM->GridToWorld(GridPos);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARoadTile* Tile = World->SpawnActor<ARoadTile>(RoadTileClass, WorldPos, FRotator::ZeroRotator, SpawnParams);
	if (!Tile)
	{
		return nullptr;
	}

	if (!Tile->PlaceOnGrid(GridPos))
	{
		Tile->Destroy();
		return nullptr;
	}

	return Tile;
}

ABuilding* ULSystemManager::FindNearestUnconnectedBuilding(const FGridVector& From) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return nullptr;
	}

	TArray<ABuilding*> Buildings = GetAllBuildings();

	ABuilding* Nearest = nullptr;
	int32 NearestDist = MAX_int32;

	for (ABuilding* Building : Buildings)
	{
		if (!Building || IsBuildingConnected(Building))
		{
			continue;
		}

		int32 Dist = MAX_int32;
		for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
		{
			Dist = FMath::Min(Dist,
				FMath::Abs(From.X - DoorwayPos.X) + FMath::Abs(From.Y - DoorwayPos.Y));
		}

		if (Dist < NearestDist)
		{
			NearestDist = Dist;
			Nearest = Building;
		}
	}

	return Nearest;
}

float ULSystemManager::GetAttractionScore(const FGridVector& From, ABuilding* Target, EGridDirection Dir) const
{
	if (!Target)
	{
		return 0.5f;
	}

	const FGridVector DirVec = GridDirectionUtils::GetVector(Dir);
	const FGridVector NextPos = From + DirVec;

	FGridVector TargetPos = Target->GetGridPosition();
	int32 BestDoorwayDistance = MAX_int32;
	for (const FGridVector& DoorwayPos : Target->GetDoorwayWorldPositions())
	{
		const int32 DoorwayDistance =
			FMath::Abs(From.X - DoorwayPos.X) + FMath::Abs(From.Y - DoorwayPos.Y);
		if (DoorwayDistance < BestDoorwayDistance)
		{
			BestDoorwayDistance = DoorwayDistance;
			TargetPos = DoorwayPos;
		}
	}

	const float DX = static_cast<float>(TargetPos.X - NextPos.X);
	const float DY = static_cast<float>(TargetPos.Y - NextPos.Y);
	const float Dist = FMath::Sqrt(DX * DX + DY * DY);

	const float DistScore = 1.0f / (1.0f + Dist);

	const float AlignDX = static_cast<float>(TargetPos.X - From.X);
	const float AlignDY = static_cast<float>(TargetPos.Y - From.Y);
	const float AlignLen = FMath::Sqrt(AlignDX * AlignDX + AlignDY * AlignDY);

	float AlignScore = 0.0f;
	if (AlignLen > KINDA_SMALL_NUMBER)
	{
		const float DirX = static_cast<float>(DirVec.X);
		const float DirY = static_cast<float>(DirVec.Y);
		AlignScore = (AlignDX * DirX + AlignDY * DirY) / AlignLen;
		AlignScore = FMath::Max(0.0f, AlignScore);
	}

	return FMath::Lerp(DistScore, AlignScore, AttractionStrength);
}

bool ULSystemManager::IsBuildingConnected(ABuilding* Building) const
{
	return Building && IsBuildingConnectedToComponent(Building, GetPrimaryRoadComponent());
}

bool ULSystemManager::AreAllBuildingsConnected() const
{
	const TArray<ABuilding*> Buildings = GetAllBuildings();
	if (Buildings.Num() == 0)
	{
		return true;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	TSet<FGridVector> Unvisited;
	for (const FGridVector& RoadPos : GM->GetCellsOfType(ECellType::Road))
	{
		Unvisited.Add(RoadPos);
	}

	while (!Unvisited.IsEmpty())
	{
		auto It = Unvisited.CreateConstIterator();
		const FGridVector Seed = *It;
		const TSet<FGridVector> Component = FloodRoadComponent(Seed, Unvisited);
		for (const FGridVector& Cell : Component)
		{
			Unvisited.Remove(Cell);
		}

		bool bContainsEveryBuilding = true;
		for (const ABuilding* Building : Buildings)
		{
			if (Building && !IsBuildingConnectedToComponent(Building, Component))
			{
				bContainsEveryBuilding = false;
				break;
			}
		}
		if (bContainsEveryBuilding)
		{
			return true;
		}
	}

	return false;
}

TArray<ABuilding*> ULSystemManager::GetAllBuildings() const
{
	TArray<ABuilding*> Result;

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return Result;
	}

	const TArray<FGridVector> BuildingCells = GM->GetCellsOfType(ECellType::Building);

	TSet<int32> SeenIDs;

	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GM->GetCell(CellPos);
		if (Cell.BuildingID == INDEX_NONE)
		{
			continue;
		}

		if (SeenIDs.Contains(Cell.BuildingID))
		{
			continue;
		}

		SeenIDs.Add(Cell.BuildingID);

		ABuilding* Building = Cast<ABuilding>(Cell.RoadActor);
		if (Building)
		{
			Result.Add(Building);
		}
	}

	return Result;
}

TSet<FGridVector> ULSystemManager::FloodRoadComponent(
	const FGridVector& Seed, const TSet<FGridVector>& RoadCells) const
{
	// Connectivity is evaluated on reciprocal connection-mask edges rather than
	// geometric adjacency. Two touching road meshes are not connected unless both
	// cells explicitly expose the corresponding direction bit.
	TSet<FGridVector> Component;
	if (!RoadCells.Contains(Seed))
	{
		return Component;
	}

	TArray<FGridVector> Queue;
	Queue.Add(Seed);
	Component.Add(Seed);
	int32 Head = 0;
	while (Head < Queue.Num())
	{
		const FGridVector Current = Queue[Head++];
		for (const EGridDirection Dir : GridDirectionUtils::GetAllDirections())
		{
			const FGridVector Neighbor = Current + GridDirectionUtils::GetVector(Dir);
			if (RoadCells.Contains(Neighbor) && !Component.Contains(Neighbor))
			{
				Component.Add(Neighbor);
				Queue.Add(Neighbor);
			}
		}
	}
	return Component;
}

bool ULSystemManager::IsBuildingConnectedToComponent(
	const ABuilding* Building, const TSet<FGridVector>& Component) const
{
	if (!Building || Component.IsEmpty())
	{
		return false;
	}

	for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
	{
		if (Component.Contains(DoorwayPos))
		{
			return true;
		}
	}
	return false;
}

TSet<FGridVector> ULSystemManager::GetPrimaryRoadComponent() const
{
	TSet<FGridVector> BestComponent;
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return BestComponent;
	}

	TSet<FGridVector> Unvisited;
	for (const FGridVector& RoadPos : GM->GetCellsOfType(ECellType::Road))
	{
		Unvisited.Add(RoadPos);
	}

	const TArray<ABuilding*> Buildings = GetAllBuildings();
	int32 BestAttachedBuildings = -1;
	while (!Unvisited.IsEmpty())
	{
		auto It = Unvisited.CreateConstIterator();
		const FGridVector Seed = *It;
		const TSet<FGridVector> Component = FloodRoadComponent(Seed, Unvisited);
		for (const FGridVector& Cell : Component)
		{
			Unvisited.Remove(Cell);
		}

		int32 AttachedBuildings = 0;
		for (const ABuilding* Building : Buildings)
		{
			if (IsBuildingConnectedToComponent(Building, Component))
			{
				++AttachedBuildings;
			}
		}

		if (AttachedBuildings > BestAttachedBuildings
			|| (AttachedBuildings == BestAttachedBuildings && Component.Num() > BestComponent.Num()))
		{
			BestAttachedBuildings = AttachedBuildings;
			BestComponent = Component;
		}
	}

	return BestComponent;
}

void ULSystemManager::ExpandNetworkThroughRoads(
	TSet<FGridVector>& Network, const TSet<FGridVector>& RoadCells) const
{
	TArray<FGridVector> Queue = Network.Array();
	int32 Head = 0;
	while (Head < Queue.Num())
	{
		const FGridVector Current = Queue[Head++];
		for (const EGridDirection Dir : GridDirectionUtils::GetAllDirections())
		{
			const FGridVector Neighbor = Current + GridDirectionUtils::GetVector(Dir);
			if (RoadCells.Contains(Neighbor) && !Network.Contains(Neighbor))
			{
				Network.Add(Neighbor);
				Queue.Add(Neighbor);
			}
		}
	}
}

bool ULSystemManager::FindPathToNetwork(
	const ABuilding* Building,
	const TSet<FGridVector>& Network,
	TArray<FGridVector>& OutPath) const
{
	// This search targets any cell in the current connected network. Its cost model
	// charges only empty cells, making reuse of player roads preferable and keeping
	// the reserved budget equal to the number of tiles that will actually be placed.
	OutPath.Reset();
	UGridManager* GM = GetGridManager();
	if (!Building || !GM || Network.IsEmpty())
	{
		return false;
	}

	TArray<FGridVector> Queue;
	TSet<FGridVector> Visited;
	TMap<FGridVector, FGridVector> CameFrom;
	for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
	{
		if (!GM->IsValidGridPos(DoorwayPos)
			|| GM->GetCellType(DoorwayPos) == ECellType::Building)
		{
			continue;
		}
		if (Network.Contains(DoorwayPos))
		{
			return true;
		}
		if (!Visited.Contains(DoorwayPos))
		{
			Visited.Add(DoorwayPos);
			Queue.Add(DoorwayPos);
		}
	}

	int32 Head = 0;
	FGridVector Goal;
	bool bFoundGoal = false;
	while (Head < Queue.Num() && !bFoundGoal)
	{
		const FGridVector Current = Queue[Head++];
		for (const EGridDirection Dir : GridDirectionUtils::GetAllDirections())
		{
			const FGridVector Neighbor = Current + GridDirectionUtils::GetVector(Dir);
			if (!GM->IsValidGridPos(Neighbor) || Visited.Contains(Neighbor)
				|| GM->GetCellType(Neighbor) == ECellType::Building)
			{
				continue;
			}

			Visited.Add(Neighbor);
			CameFrom.Add(Neighbor, Current);
			if (Network.Contains(Neighbor))
			{
				Goal = Neighbor;
				bFoundGoal = true;
				break;
			}
			Queue.Add(Neighbor);
		}
	}

	if (!bFoundGoal)
	{
		return false;
	}

	// Reconstruct from the existing network out toward the building doorway so
	// the animated placement grows as one continuous connection.
	FGridVector Current = Goal;
	OutPath.Add(Current);
	while (const FGridVector* Previous = CameFrom.Find(Current))
	{
		Current = *Previous;
		OutPath.Add(Current);
	}
	return true;
}

void ULSystemManager::BuildConnectionPlan()
{
	// Reserve complete paths before optional growth. Each accepted path expands the
	// working network, so later buildings can connect to earlier reserved paths and
	// share segments instead of paying for independent routes.
	UGridManager* GM = GetGridManager();
	PendingConnectionCells.Reset();
	NextConnectionCellIndex = 0;
	if (!GM || GetGenerationBudgetRemaining() <= 0)
	{
		return;
	}

	const TArray<ABuilding*> Buildings = GetAllBuildings();
	if (Buildings.IsEmpty())
	{
		return;
	}

	TSet<FGridVector> RoadCells;
	for (const FGridVector& RoadPos : GM->GetCellsOfType(ECellType::Road))
	{
		RoadCells.Add(RoadPos);
	}
	TSet<FGridVector> Network = GetPrimaryRoadComponent();
	int32 PlannedCost = 0;
	const int32 AvailableBudget = GetGenerationBudgetRemaining();

	// With no arterial road, seed the network at one valid building doorway.
	if (Network.IsEmpty())
	{
		for (const ABuilding* Building : Buildings)
		{
			if (!Building) continue;
			for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
			{
				if (GM->IsValidGridPos(DoorwayPos)
					&& GM->GetCellType(DoorwayPos) == ECellType::Empty)
				{
					PendingConnectionCells.Add(DoorwayPos);
					RoadCells.Add(DoorwayPos);
					Network.Add(DoorwayPos);
					++PlannedCost;
					break;
				}
			}
			if (!Network.IsEmpty()) break;
		}
	}

	for (int32 Iteration = 0; Iteration < Buildings.Num() && PlannedCost <= AvailableBudget; ++Iteration)
	{
		ABuilding* BestBuilding = nullptr;
		TArray<FGridVector> BestPath;
		int32 BestNewCellCount = MAX_int32;

		for (ABuilding* Building : Buildings)
		{
			if (!Building || IsBuildingConnectedToComponent(Building, Network))
			{
				continue;
			}

			TArray<FGridVector> CandidatePath;
			if (!FindPathToNetwork(Building, Network, CandidatePath))
			{
				continue;
			}

			int32 NewCellCount = 0;
			for (const FGridVector& Cell : CandidatePath)
			{
				if (!RoadCells.Contains(Cell)) ++NewCellCount;
			}
			if (NewCellCount < BestNewCellCount)
			{
				BestBuilding = Building;
				BestPath = MoveTemp(CandidatePath);
				BestNewCellCount = NewCellCount;
			}
		}

		if (!BestBuilding || PlannedCost + BestNewCellCount > AvailableBudget)
		{
			break;
		}

		for (const FGridVector& Cell : BestPath)
		{
			if (!RoadCells.Contains(Cell))
			{
				PendingConnectionCells.Add(Cell);
				RoadCells.Add(Cell);
				++PlannedCost;
			}
			Network.Add(Cell);
		}
		ExpandNetworkThroughRoads(Network, RoadCells);
	}
}

int32 ULSystemManager::CountPendingConnectionCost() const
{
	const UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return 0;
	}

	int32 Cost = 0;
	for (int32 Index = NextConnectionCellIndex; Index < PendingConnectionCells.Num(); ++Index)
	{
		if (GM->GetCellType(PendingConnectionCells[Index]) == ECellType::Empty)
		{
			++Cost;
		}
	}
	return Cost;
}

int32 ULSystemManager::GetGenerationBudgetRemaining() const
{
	const UGridManager* GM = GetGridManager();
	const int32 LocalRemaining = FMath::Max(0, BranchBudget - CellsPlacedThisGeneration);
	return GM ? FMath::Min(LocalRemaining, GM->GetRemainingBudget()) : 0;
}

bool ULSystemManager::CanGrowInDirection(const FGridVector& Pos, EGridDirection Dir) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const FGridVector ForwardPos = Pos + GridDirectionUtils::GetVector(Dir);

	if (!GM->IsValidGridPos(ForwardPos))
	{
		return false;
	}

	return GM->GetCellType(ForwardPos) == ECellType::Empty;
}

EGridDirection ULSystemManager::TurnLeft(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:    return EGridDirection::Left;
	case EGridDirection::Left:  return EGridDirection::Down;
	case EGridDirection::Down:  return EGridDirection::Right;
	case EGridDirection::Right: return EGridDirection::Up;
	default:                    return EGridDirection::None;
	}
}

EGridDirection ULSystemManager::TurnRight(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:    return EGridDirection::Right;
	case EGridDirection::Right: return EGridDirection::Down;
	case EGridDirection::Down:  return EGridDirection::Left;
	case EGridDirection::Left:  return EGridDirection::Up;
	default:                    return EGridDirection::None;
	}
}

EGridDirection ULSystemManager::OppositeDirection(EGridDirection Dir)
{
	switch (Dir)
	{
	case EGridDirection::Up:    return EGridDirection::Down;
	case EGridDirection::Down:  return EGridDirection::Up;
	case EGridDirection::Left:  return EGridDirection::Right;
	case EGridDirection::Right: return EGridDirection::Left;
	default:                    return EGridDirection::None;
	}
}

int32 ULSystemManager::PopCount(uint8 Mask)
{
	int32 Count = 0;
	if (Mask & 0x01) ++Count;
	if (Mask & 0x02) ++Count;
	if (Mask & 0x04) ++Count;
	if (Mask & 0x08) ++Count;
	return Count;
}

bool ULSystemManager::IsStraightRoad(uint8 Mask)
{
	return PopCount(Mask) == 2
		&& (Mask == ((static_cast<uint8>(EGridDirection::Up) | static_cast<uint8>(EGridDirection::Down)))
		 || Mask == ((static_cast<uint8>(EGridDirection::Left) | static_cast<uint8>(EGridDirection::Right))));
}

bool ULSystemManager::IsDeadEnd(uint8 Mask)
{
	return PopCount(Mask) == 1;
}

bool ULSystemManager::IsSideBranchValid(const FGridVector& Pos, EGridDirection Dir) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const EGridDirection SideA = TurnLeft(Dir);
	const EGridDirection SideB = TurnRight(Dir);

	const FGridVector BranchTarget = Pos + GridDirectionUtils::GetVector(Dir);
	const FGridVector CheckA = BranchTarget + GridDirectionUtils::GetVector(SideA);
	const FGridVector CheckB = BranchTarget + GridDirectionUtils::GetVector(SideB);

	const ECellType TypeA = GM->IsValidGridPos(CheckA) ? GM->GetCellType(CheckA) : ECellType::Empty;
	const ECellType TypeB = GM->IsValidGridPos(CheckB) ? GM->GetCellType(CheckB) : ECellType::Empty;

	return TypeA != ECellType::Road && TypeB != ECellType::Road;
}

void ULSystemManager::FinishGeneration(bool bAllConnected)
{
	// Always clear transient queues before broadcasting. Listeners may immediately
	// start Simulation or another preview match, so no state from this run may leak
	// into the next generation request.
	GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
	PendingGrowthPoints.Empty();
	QueuedGrowthPoints.Empty();
	PendingConnectionCells.Empty();
	NextConnectionCellIndex = 0;
	RemainingBudget = GetGenerationBudgetRemaining();
	bIsGenerating = false;
	UE_LOG(LogCityFlowLSystem, Log,
		TEXT("Generation finished: connected=%s, cells placed=%d, local budget remaining=%d"),
		bAllConnected ? TEXT("true") : TEXT("false"), CellsPlacedThisGeneration, RemainingBudget);
	OnGenerationFinished.Broadcast(bAllConnected);
}
