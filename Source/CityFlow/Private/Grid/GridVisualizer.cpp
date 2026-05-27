#include "Grid/GridVisualizer.h"
#include "Grid/GridManager.h"
#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

AGridVisualizer::AGridVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	LineBatchComponent = CreateDefaultSubobject<ULineBatchComponent>(TEXT("LineBatchComponent"));
	RootComponent = LineBatchComponent;
}

void AGridVisualizer::BeginPlay()
{
	Super::BeginPlay();

	if (bShowOnBeginPlay)
	{
		DrawGrid();
	}
}

void AGridVisualizer::DrawGrid()
{
	if (bGridDrawn)
	{
		return;
	}

	DrawGridInternal();
	bGridVisible = true;
	bGridDrawn = true;
}

void AGridVisualizer::ClearGrid()
{
	if (!bGridDrawn)
	{
		return;
	}

	LineBatchComponent->Flush();
	bGridVisible = false;
	bGridDrawn = false;
}

void AGridVisualizer::RedrawGrid()
{
	LineBatchComponent->Flush();
	bGridDrawn = false;
	DrawGridInternal();
	bGridVisible = true;
	bGridDrawn = true;
}

void AGridVisualizer::SetGridVisible(bool bVisible)
{
	if (bVisible && !bGridVisible)
	{
		DrawGrid();
	}
	else if (!bVisible && bGridVisible)
	{
		ClearGrid();
	}
}

void AGridVisualizer::DrawGridInternal()
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const int32 Width = GM->GetGridWidth();
	const int32 Height = GM->GetGridHeight();
	const float CellSizeVal = GM->GetCellSize();
	const FVector Origin = GM->GetGridOrigin()-FVector(CellSizeVal*0.5f, CellSizeVal*0.5f, 0.0f);
	const float Z = Origin.Z + GridLineZOffset;

	const float TotalWidth = Width * CellSizeVal;
	const float TotalHeight = Height * CellSizeVal;
	const uint8 DepthPriority = SDPG_World;
	const float LifeTime = -1.0f;

	for (int32 X = 0; X <= Width; ++X)
	{
		const float WorldX = Origin.X + X * CellSizeVal;
		FVector Start(WorldX, Origin.Y, Z);
		FVector End(WorldX, Origin.Y + TotalHeight, Z);
		LineBatchComponent->DrawLine(Start, End, GridLineColor, DepthPriority, GridLineThickness, LifeTime);
	}

	for (int32 Y = 0; Y <= Height; ++Y)
	{
		const float WorldY = Origin.Y + Y * CellSizeVal;
		FVector Start(Origin.X, WorldY, Z);
		FVector End(Origin.X + TotalWidth, WorldY, Z);
		LineBatchComponent->DrawLine(Start, End, GridLineColor, DepthPriority, GridLineThickness, LifeTime);
	}
}

UGridManager* AGridVisualizer::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
