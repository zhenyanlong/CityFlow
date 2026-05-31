#include "Grid/GridPlaceableActor.h"
#include "Grid/GridManager.h"
#include "Engine/World.h"

AGridPlaceableActor::AGridPlaceableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;
}

void AGridPlaceableActor::BeginPlay()
{
	Super::BeginPlay();
}

void AGridPlaceableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsPlaced)
	{
		UnregisterCells();
	}
	Super::EndPlay(EndPlayReason);
}

void AGridPlaceableActor::EnterPreviewState()
{
	if (bIsPlaced)
	{
		return;
	}

	bIsPreview = true;
	OnEnterPreview();
}

void AGridPlaceableActor::EnterPlacedState()
{
	bIsPreview = false;
	bIsPlaced = true;
	OnEnterPlaced();
}

bool AGridPlaceableActor::PlaceOnGrid(const FGridVector& InGridPos)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	if (bIsPlaced)
	{
		RemoveFromGrid();
	}

	if (!CanPlaceAt(InGridPos))
	{
		return false;
	}

	GridPosition = InGridPos;
	OccupiedCells = CalculateOccupiedCells(InGridPos);

	if (!RegisterCells())
	{
		for (const FGridVector& Cell : OccupiedCells)
		{
			GM->ClearCell(Cell);
		}
		OccupiedCells.Empty();
		GridPosition = FGridVector();
		return false;
	}

	const FVector WorldPos = GetGridWorldPosition();
	SetActorLocation(WorldPos);

	EnterPlacedState();
	OnPlacedOnGrid();
	GM->OnGridPlaced.Broadcast(this);

	return true;
}

bool AGridPlaceableActor::PlaceOnGridAtWorld(const FVector& WorldLocation)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	const FGridVector GridPos = GM->WorldToGrid(WorldLocation);
	return PlaceOnGrid(GridPos);
}

void AGridPlaceableActor::RemoveFromGrid()
{
	if (!bIsPlaced)
	{
		return;
	}

	UnregisterCells();
	bIsPlaced = false;
	OccupiedCells.Empty();

	OnRemovedFromGrid();
}

bool AGridPlaceableActor::CanPlaceAt(const FGridVector& InGridPos) const
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	const TArray<FGridVector> Cells = CalculateOccupiedCells(InGridPos);

	for (const FGridVector& Cell : Cells)
	{
		if (!GM->IsValidGridPos(Cell))
		{
			return false;
		}

		if (!bIsPlaced || !OccupiedCells.Contains(Cell))
		{
			const FGridCell& ExistingCell = GM->GetCell(Cell);
			if (ExistingCell.Type != ECellType::Empty)
			{
				return false;
			}
		}
	}

	return ValidatePlacement(InGridPos);
}

bool AGridPlaceableActor::SnapToGridPosition(const FVector& WorldLocation)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return false;
	}

	const FVector SnappedPos = GM->SnapToGrid(WorldLocation);
	SetActorLocation(SnappedPos);
	return true;
}

FVector AGridPlaceableActor::GetGridWorldPosition() const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return GetActorLocation();
	}
	return GM->GridToWorld(GridPosition);
}

TArray<FGridVector> AGridPlaceableActor::CalculateOccupiedCells(const FGridVector& BasePos) const
{
	TArray<FGridVector> Cells;
	const int32 SizeX = FMath::Max(1, FMath::RoundToInt32(BuildingSize.X));
	const int32 SizeY = FMath::Max(1, FMath::RoundToInt32(BuildingSize.Y));

	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			Cells.Add(FGridVector(BasePos.X + X, BasePos.Y + Y));
		}
	}

	return Cells;
}

bool AGridPlaceableActor::ValidatePlacement(const FGridVector& BasePos) const
{
	return true;
}

UGridManager* AGridPlaceableActor::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}

bool AGridPlaceableActor::RegisterCells()
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const ECellType CellType = GetPlacementCellType();
	for (const FGridVector& Cell : OccupiedCells)
	{
		if (!GM->OccupyCell(Cell, CellType, GetUniqueID(), this))
		{
			return false;
		}
	}

	return true;
}

void AGridPlaceableActor::UnregisterCells()
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	for (const FGridVector& Cell : OccupiedCells)
	{
		GM->ClearCell(Cell);
	}
}

void AGridPlaceableActor::OnEnterPreview_Implementation()
{
}

void AGridPlaceableActor::OnEnterPlaced_Implementation()
{
}

void AGridPlaceableActor::OnPlacedOnGrid_Implementation()
{
}

void AGridPlaceableActor::OnRemovedFromGrid_Implementation()
{
}

void AGridPlaceableActor::SetPreviewPlacementValid(bool bValid)
{
	if (bPreviewPlacementValid != bValid)
	{
		bPreviewPlacementValid = bValid;
		OnPreviewValidChanged(bValid);
	}
}

void AGridPlaceableActor::OnPreviewValidChanged_Implementation(bool bValid)
{
}

void AGridPlaceableActor::UpdatePreviewAppearance(const FGridVector& GridPos)
{
}
