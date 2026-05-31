#include "LSystem/Subsystem/LSystemManager.h"
#include "Grid/GridManager.h"
#include "Grid/RoadTile.h"
#include "Grid/Building.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	BranchBudget = FMath::Max(1, NewBudget);
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
	RemainingBudget = FMath::Max(0, RemainingBudget + Amount);
}

void ULSystemManager::StartGenerate()
{
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

	RemainingBudget = GM->GetRemainingBudget();
	BranchBudget = RemainingBudget;
	bIsGenerating = true;

	PendingGrowthPoints.Empty();
	CollectStartPoints();

	OnGenerationStarted.Broadcast();

	if (PendingGrowthPoints.Num() == 0 || RemainingBudget <= 0)
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

	const bool bAllConnected = AreAllBuildingsConnected();
	bIsGenerating = false;
	OnGenerationFinished.Broadcast(bAllConnected);
}

void ULSystemManager::CollectStartPoints()
{
	CollectStartPointsFromRoads();
	CollectStartPointsFromBuildings();
}

void ULSystemManager::CollectStartPointsFromRoads()
{
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

			for (EGridDirection Dir : AllDirs)
			{
				const uint8 DirBit = static_cast<uint8>(Dir);
				if (Mask & DirBit)
				{
					continue;
				}

				const FGridVector NeighborPos = RoadPos + GridDirectionUtils::GetVector(Dir);
				if (GM->IsValidGridPos(NeighborPos) && GM->GetCellType(NeighborPos) == ECellType::Empty)
				{
					PendingGrowthPoints.Add(FLSystemGrowthPoint(RoadPos, Dir));
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
			while (GM->IsValidGridPos(Walker) && GM->GetCellType(Walker) == ECellType::Road)
			{
				SegmentCells.Insert(Walker, 0);
				Walker = Walker + GridDirectionUtils::GetVector(AxisDirA);
			}

			Walker = RoadPos + GridDirectionUtils::GetVector(AxisDirB);
			while (GM->IsValidGridPos(Walker) && GM->GetCellType(Walker) == ECellType::Road)
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
					PendingGrowthPoints.Add(FLSystemGrowthPoint(SamplePos, PerpA));
				}

				if (GM->IsValidGridPos(SamplePos + GridDirectionUtils::GetVector(PerpB))
					&& GM->GetCellType(SamplePos + GridDirectionUtils::GetVector(PerpB)) == ECellType::Empty)
				{
					PendingGrowthPoints.Add(FLSystemGrowthPoint(SamplePos, PerpB));
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
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	TArray<ABuilding*> Buildings = GetAllBuildings();
	for (ABuilding* Building : Buildings)
	{
		if (!Building || IsBuildingConnected(Building))
		{
			continue;
		}

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

			PendingGrowthPoints.Add(FLSystemGrowthPoint(BasePos, Doorway.FacingDirection));
		}
	}
}

void ULSystemManager::ProcessGrowthStep()
{
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

	RemainingBudget = GM->GetRemainingBudget();

	if (PendingGrowthPoints.Num() == 0 || RemainingBudget <= 0)
	{
		FinishGeneration(AreAllBuildingsConnected());
		return;
	}

	if (AreAllBuildingsConnected())
	{
		FinishGeneration(true);
		return;
	}

	FLSystemGrowthPoint Point = PendingGrowthPoints[0];
	PendingGrowthPoints.RemoveAt(0);

	TryGrowAt(Point);

	RemainingBudget = GM->GetRemainingBudget();
	OnGenerationStep.Broadcast(RemainingBudget);

	if (PendingGrowthPoints.Num() == 0 || RemainingBudget <= 0)
	{
		FinishGeneration(AreAllBuildingsConnected());
		return;
	}

	if (AreAllBuildingsConnected())
	{
		FinishGeneration(true);
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

bool ULSystemManager::TryGrowAt(const FLSystemGrowthPoint& Point)
{
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

		if (GM->GetRemainingBudget() <= 0)
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

			if (FMath::FRand() < BranchProbability)
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
		PendingGrowthPoints.Add(NewPt);
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

		const FGridVector BuildingPos = Building->GetGridPosition();
		const int32 Dist = FMath::Abs(From.X - BuildingPos.X) + FMath::Abs(From.Y - BuildingPos.Y);

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

	const FGridVector TargetPos = Target->GetGridPosition();

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
	if (!Building)
	{
		return false;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const TArray<FGridVector> DoorwayPositions = Building->GetDoorwayWorldPositions();
	for (const FGridVector& DoorwayPos : DoorwayPositions)
	{
		if (GM->GetCellType(DoorwayPos) == ECellType::Road)
		{
			return true;
		}
	}

	return false;
}

bool ULSystemManager::AreAllBuildingsConnected() const
{
	TArray<ABuilding*> Buildings = GetAllBuildings();
	if (Buildings.Num() == 0)
	{
		return true;
	}

	for (ABuilding* Building : Buildings)
	{
		if (Building && !IsBuildingConnected(Building))
		{
			return false;
		}
	}

	return true;
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
	GetWorld()->GetTimerManager().ClearTimer(GrowthTimerHandle);
	PendingGrowthPoints.Empty();
	bIsGenerating = false;
	OnGenerationFinished.Broadcast(bAllConnected);
}
