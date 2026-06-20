#include "Showcase/Actor/BuildingShowcaseDirector.h"

#include "Components/SceneComponent.h"
#include "GameMode/Types/BuildingDataAsset.h"
#include "Grid/Building.h"
#include "Grid/GridManager.h"
#include "Grid/GridVisualizer.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogBuildingShowcase, Log, All);

namespace
{
	struct FShowcasePlacement
	{
		TSubclassOf<ABuilding> BuildingClass;
		EGridRotation Rotation = EGridRotation::Rot0;
		FGridVector GridPosition;
		FIntPoint EffectiveSize = FIntPoint(1, 1);
	};

	FIntPoint GetEffectiveSize(const ABuilding* BuildingCDO, EGridRotation Rotation)
	{
		const FVector2D BaseSize = BuildingCDO ? BuildingCDO->GetBuildingSize() : FVector2D(1.0f, 1.0f);
		const int32 Width = FMath::Max(1, FMath::RoundToInt32(BaseSize.X));
		const int32 Height = FMath::Max(1, FMath::RoundToInt32(BaseSize.Y));
		const bool bSwapped = Rotation == EGridRotation::Rot90 || Rotation == EGridRotation::Rot270;
		return bSwapped ? FIntPoint(Height, Width) : FIntPoint(Width, Height);
	}
}

ABuildingShowcaseDirector::ABuildingShowcaseDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	RootComponent = RootSceneComponent;
	GridVisualizerClass = AGridVisualizer::StaticClass();
}

void ABuildingShowcaseDirector::BeginPlay()
{
	Super::BeginPlay();

	if (bBuildOnBeginPlay)
	{
		BuildShowcase();
	}
}

void ABuildingShowcaseDirector::BuildShowcase()
{
	UWorld* World = GetWorld();
	UGridManager* GridManager = World ? World->GetSubsystem<UGridManager>() : nullptr;
	if (!World || !GridManager)
	{
		return;
	}

	ClearShowcase();

	TArray<TSubclassOf<ABuilding>> BuildingClasses;
	if (BuildingDataAsset)
	{
		for (const FBuildingDataEntry& Entry : BuildingDataAsset->BuildingEntries)
		{
			if (Entry.BuildingClass)
			{
				BuildingClasses.Add(Entry.BuildingClass);
			}
		}
	}
	for (const TSubclassOf<ABuilding>& BuildingClass : AdditionalBuildingClasses)
	{
		if (BuildingClass)
		{
			BuildingClasses.Add(BuildingClass);
		}
	}

	if (bDeduplicateBuildingClasses)
	{
		TSet<UClass*> SeenClasses;
		BuildingClasses.RemoveAll([&SeenClasses](const TSubclassOf<ABuilding>& BuildingClass)
		{
			UClass* Class = BuildingClass.Get();
			if (!Class || SeenClasses.Contains(Class))
			{
				return true;
			}
			SeenClasses.Add(Class);
			return false;
		});
	}

	static const EGridRotation AllRotations[] = {
		EGridRotation::Rot0,
		EGridRotation::Rot90,
		EGridRotation::Rot180,
		EGridRotation::Rot270
	};

	TArray<FShowcasePlacement> Placements;
	const int32 SafePadding = FMath::Max(1, PaddingCells);
	int32 ActualGridWidth = FMath::Max(4, GridWidth);

	for (const TSubclassOf<ABuilding>& BuildingClass : BuildingClasses)
	{
		const ABuilding* BuildingCDO = BuildingClass ? BuildingClass->GetDefaultObject<ABuilding>() : nullptr;
		if (!BuildingCDO)
		{
			continue;
		}

		const int32 RotationCount = bSpawnAllRotations ? UE_ARRAY_COUNT(AllRotations) : 1;
		for (int32 RotationIndex = 0; RotationIndex < RotationCount; ++RotationIndex)
		{
			FShowcasePlacement& Placement = Placements.AddDefaulted_GetRef();
			Placement.BuildingClass = BuildingClass;
			Placement.Rotation = bSpawnAllRotations ? AllRotations[RotationIndex] : SingleRotation;
			Placement.EffectiveSize = GetEffectiveSize(BuildingCDO, Placement.Rotation);
			if (bAutoExpandGridToFit)
			{
				ActualGridWidth = FMath::Max(ActualGridWidth, Placement.EffectiveSize.X + SafePadding * 2);
			}
		}
	}

	int32 CursorX = SafePadding;
	int32 CursorY = SafePadding;
	int32 RowHeight = 0;
	for (FShowcasePlacement& Placement : Placements)
	{
		if (CursorX + Placement.EffectiveSize.X + SafePadding > ActualGridWidth)
		{
			CursorX = SafePadding;
			CursorY += RowHeight + SafePadding;
			RowHeight = 0;
		}

		Placement.GridPosition = FGridVector(CursorX, CursorY);
		CursorX += Placement.EffectiveSize.X + SafePadding;
		RowHeight = FMath::Max(RowHeight, Placement.EffectiveSize.Y);
	}

	const int32 RequiredHeight = CursorY + RowHeight + SafePadding;
	const int32 ActualGridHeight = bAutoExpandGridToFit
		? FMath::Max(FMath::Max(4, GridHeight), RequiredHeight)
		: FMath::Max(4, GridHeight);
	const FVector GridOrigin = GetActorLocation() + GridOriginOffset;
	GridManager->InitGrid(ActualGridWidth, ActualGridHeight, FMath::Max(10.0f, CellSize), GridOrigin);
	GridManager->SetRoadBudget(0);

	int32 FailedCount = 0;
	for (const FShowcasePlacement& Placement : Placements)
	{
		if (Placement.GridPosition.X + Placement.EffectiveSize.X > ActualGridWidth
			|| Placement.GridPosition.Y + Placement.EffectiveSize.Y > ActualGridHeight)
		{
			++FailedCount;
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABuilding* Building = World->SpawnActor<ABuilding>(
			Placement.BuildingClass,
			GridManager->GridToWorld(Placement.GridPosition),
			FRotator::ZeroRotator,
			SpawnParams);
		if (!Building)
		{
			++FailedCount;
			continue;
		}

		Building->BuildingRotation = Placement.Rotation;
		Building->bPlaySpawnAnimation = bPlaySpawnAnimations;
		if (Building->PlaceOnGrid(Placement.GridPosition))
		{
			SpawnedBuildings.Add(Building);
		}
		else
		{
			++FailedCount;
			Building->Destroy();
		}
	}

	if (bSpawnGridVisualizer && GridVisualizerClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedGridVisualizer = World->SpawnActor<AGridVisualizer>(
			GridVisualizerClass, GridOrigin, FRotator::ZeroRotator, SpawnParams);
	}

	UE_LOG(LogBuildingShowcase, Log, TEXT("Building showcase placed %d buildings; %d failed."),
		SpawnedBuildings.Num(), FailedCount);
	OnShowcaseBuilt.Broadcast(SpawnedBuildings.Num(), FailedCount);
}

TArray<ABuilding*> ABuildingShowcaseDirector::GetSpawnedBuildings() const
{
	TArray<ABuilding*> Result;
	Result.Reserve(SpawnedBuildings.Num());
	for (ABuilding* Building : SpawnedBuildings)
	{
		if (IsValid(Building))
		{
			Result.Add(Building);
		}
	}
	return Result;
}

void ABuildingShowcaseDirector::ClearShowcase()
{
	for (ABuilding* Building : SpawnedBuildings)
	{
		if (IsValid(Building))
		{
			Building->RemoveFromGrid();
			Building->Destroy();
		}
	}
	SpawnedBuildings.Reset();

	if (IsValid(SpawnedGridVisualizer))
	{
		SpawnedGridVisualizer->Destroy();
	}
	SpawnedGridVisualizer = nullptr;
}
