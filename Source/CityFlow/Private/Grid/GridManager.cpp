#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"

void UGridManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGridManager::Deinitialize()
{
	Grid.Empty();
	Super::Deinitialize();
}

void UGridManager::InitGrid(int32 InGridWidth, int32 InGridHeight, float InCellSize, const FVector& InGridOrigin)
{
	GridWidth = InGridWidth;
	GridHeight = InGridHeight;
	CellSize = InCellSize;
	GridOrigin = FVector(InGridOrigin.X - (GridWidth - 1) * CellSize * 0.5f, InGridOrigin.Y - (GridHeight - 1) * CellSize * 0.5f, InGridOrigin.Z);

	Grid.SetNum(GridHeight);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		Grid[Y].SetNum(GridWidth);
		for (int32 X = 0; X < GridWidth; ++X)
		{
			Grid[Y][X].Type = ECellType::Empty;
			Grid[Y][X].ConnectedMask = 0;
			Grid[Y][X].BuildingID = INDEX_NONE;
			Grid[Y][X].RoadActor = nullptr;
		}
	}

	bGridInitialized = true;
}

FGridVector UGridManager::WorldToGrid(const FVector& WorldLocation) const
{
	const float X = (WorldLocation.X - GridOrigin.X) / CellSize;
	const float Y = (WorldLocation.Y - GridOrigin.Y) / CellSize;
	return FGridVector(FMath::RoundToInt32(X), FMath::RoundToInt32(Y));
}

FVector UGridManager::GridToWorld(const FGridVector& GridPos) const
{
	const float X = GridOrigin.X + GridPos.X * CellSize;
	const float Y = GridOrigin.Y + GridPos.Y * CellSize;
	return FVector(X, Y, GridOrigin.Z);
}

FVector UGridManager::SnapToGrid(const FVector& WorldLocation) const
{
	return GridToWorld(WorldToGrid(WorldLocation));
}

bool UGridManager::IsValidGridPos(const FGridVector& GridPos) const
{
	return GridPos.X >= 0 && GridPos.X < GridWidth && GridPos.Y >= 0 && GridPos.Y < GridHeight;
}

const FGridCell& UGridManager::GetCell(const FGridVector& GridPos) const
{
	static FGridCell InvalidCell;
	if (!IsValidGridPos(GridPos))
	{
		return InvalidCell;
	}
	return Grid[GridPos.Y][GridPos.X];
}

bool UGridManager::OccupyCell(const FGridVector& GridPos, ECellType Type, int32 BuildingID, AActor* RoadActor)
{
	if (!IsValidGridPos(GridPos))
	{
		return false;
	}

	FGridCell& Cell = Grid[GridPos.Y][GridPos.X];
	if (Cell.Type != ECellType::Empty && Type != ECellType::Empty)
	{
		return false;
	}

	Cell.Type = Type;
	Cell.BuildingID = BuildingID;
	Cell.RoadActor = RoadActor;

	if (Type == ECellType::Road)
	{
		Cell.ConnectedMask = CalculateConnectedMask(GridPos);
		UpdateNeighborMasks(GridPos);
	}

	OnCellChanged.Broadcast(GridPos, Cell);
	return true;
}

bool UGridManager::OccupyCell(const FGridVector& GridPos, ECellType Type)
{
	return OccupyCell(GridPos, Type, INDEX_NONE, nullptr);
}

bool UGridManager::ClearCell(const FGridVector& GridPos)
{
	if (!IsValidGridPos(GridPos))
	{
		return false;
	}

	FGridCell& Cell = Grid[GridPos.Y][GridPos.X];
	Cell.Type = ECellType::Empty;
	Cell.ConnectedMask = 0;
	Cell.BuildingID = INDEX_NONE;
	Cell.RoadActor = nullptr;

	UpdateNeighborMasks(GridPos);

	OnCellChanged.Broadcast(GridPos, Cell);
	return true;
}

ECellType UGridManager::GetCellType(const FGridVector& GridPos) const
{
	if (!IsValidGridPos(GridPos))
	{
		return ECellType::Empty;
	}
	return Grid[GridPos.Y][GridPos.X].Type;
}

TArray<FGridVector> UGridManager::GetNeighbors(const FGridVector& GridPos, bool bCardinalOnly) const
{
	TArray<FGridVector> Result;
	if (!IsValidGridPos(GridPos))
	{
		return Result;
	}

	const TArray<FGridVector> Offsets = bCardinalOnly
		? TArray<FGridVector>{ FGridVector(0, -1), FGridVector(0, 1), FGridVector(-1, 0), FGridVector(1, 0) }
		: TArray<FGridVector>{
			FGridVector(-1, -1), FGridVector(0, -1), FGridVector(1, -1),
			FGridVector(-1,  0),                    FGridVector(1,  0),
			FGridVector(-1,  1), FGridVector(0,  1), FGridVector(1,  1)
		};

	for (const FGridVector& Offset : Offsets)
	{
		FGridVector Neighbor = GridPos + Offset;
		if (IsValidGridPos(Neighbor))
		{
			Result.Add(Neighbor);
		}
	}

	return Result;
}

TArray<FGridVector> UGridManager::GetCellsOfType(ECellType Type) const
{
	TArray<FGridVector> Result;
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			if (Grid[Y][X].Type == Type)
			{
				Result.Add(FGridVector(X, Y));
			}
		}
	}
	return Result;
}

bool UGridManager::HasAdjacentType(const FGridVector& GridPos, ECellType Type) const
{
	TArray<FGridVector> Neighbors = GetNeighbors(GridPos, true);
	for (const FGridVector& N : Neighbors)
	{
		if (GetCellType(N) == Type)
		{
			return true;
		}
	}
	return false;
}

uint8 UGridManager::CalculateConnectedMask(const FGridVector& GridPos) const
{
	uint8 Mask = 0;

	if (GetCellType(GridPos + GridDirectionUtils::Up) == ECellType::Road)
	{
		Mask |= static_cast<uint8>(EGridDirection::Up);
	}
	if (GetCellType(GridPos + GridDirectionUtils::Down) == ECellType::Road)
	{
		Mask |= static_cast<uint8>(EGridDirection::Down);
	}
	if (GetCellType(GridPos + GridDirectionUtils::Left) == ECellType::Road)
	{
		Mask |= static_cast<uint8>(EGridDirection::Left);
	}
	if (GetCellType(GridPos + GridDirectionUtils::Right) == ECellType::Road)
	{
		Mask |= static_cast<uint8>(EGridDirection::Right);
	}

	return Mask;
}

void UGridManager::UpdateNeighborMasks(const FGridVector& GridPos)
{
	TArray<FGridVector> Neighbors = GetNeighbors(GridPos, true);
	for (const FGridVector& N : Neighbors)
	{
		if (Grid[N.Y][N.X].Type == ECellType::Road)
		{
			uint8 NewMask = CalculateConnectedMask(N);
			Grid[N.Y][N.X].ConnectedMask = NewMask;
			OnCellChanged.Broadcast(N, Grid[N.Y][N.X]);
		}
	}
}
