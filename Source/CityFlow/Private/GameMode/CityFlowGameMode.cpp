#include "GameMode/CityFlowGameMode.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "Grid/Building.h"
#include "Grid/RoadTile.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowGM, Log, All);

ACityFlowGameMode::ACityFlowGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACityFlowGameMode::BeginPlay()
{
	Super::BeginPlay();

	RemainingBudget = TotalRoadBudget;
	PlayerBudget = FMath::RoundToInt(TotalRoadBudget * (1.0f - LSystemBudgetShare));
	LSystemBudget = TotalRoadBudget - PlayerBudget;
	RemainingBudget = TotalRoadBudget;

	{
		UGridManager* GM = GetGridManager();
		if (GM)
		{
			GM->SetRoadBudget(TotalRoadBudget);
		}
	}

	InitializeDefaultScene();

	if (GameWidgetClass)
	{
		GameWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), GameWidgetClass);
		if (GameWidgetInstance)
		{
			GameWidgetInstance->AddToViewport();
		}
	}

	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACityFlowGameMode::InitializeDefaultScene()
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	if (!GM->IsGridInitialized())
	{
		const FVector Origin = FVector(0.0f, 0.0f, 0.0f);
		GM->InitGrid(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, Origin);
	}

	if (OriginBuildingClass || DestinationBuildingClass)
	{
		TArray<FBuildingSpawnRequest> Requests;

		const int32 HalfCount = DefaultBuildingCount / 2;

		if (OriginBuildingClass)
		{
			FBuildingSpawnRequest OriginReq;
			OriginReq.BuildingClass = OriginBuildingClass;
			OriginReq.Count = HalfCount;
			Requests.Add(OriginReq);
		}

		if (DestinationBuildingClass)
		{
			FBuildingSpawnRequest DestReq;
			DestReq.BuildingClass = DestinationBuildingClass;
			DestReq.Count = DefaultBuildingCount - HalfCount;
			Requests.Add(DestReq);
		}

		GM->TryPlaceBuildingsRandom(Requests);
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (LSM && RoadTileClass)
	{
		LSM->SetRoadTileClass(RoadTileClass);
		LSM->SetBranchBudget(LSystemBudget);
	}
}

void ACityFlowGameMode::TransitionToPhase(ECityFlowGamePhase NewPhase)
{
	const ECityFlowGamePhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	OnGamePhaseChanged.Broadcast(OldPhase, NewPhase);

	switch (NewPhase)
	{
	case ECityFlowGamePhase::Planning:
		RemainingBudget = TotalRoadBudget;
		PlayerBudget = FMath::RoundToInt(TotalRoadBudget * (1.0f - LSystemBudgetShare));
		LSystemBudget = TotalRoadBudget - PlayerBudget;

		{
			ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
			if (LSM)
			{
				LSM->SetBranchBudget(LSystemBudget);
			}
		}
		break;

	case ECityFlowGamePhase::Simulating:
	{
		UVehicleManager* VM = GetVehicleManager();
		UScoringManager* SM = GetScoringManager();

		if (VM)
		{
			VM->SetDrivingSide(DrivingSide);
			VM->SetLaneOffsetFactor(LaneOffsetFactor);
			VM->StartSpawning();
		}

		if (SM)
		{
			SM->StartScoring();
		}

		SimulationTimeRemaining = SimulationDuration;
		GetWorldTimerManager().SetTimer(
			SimulationTimerHandle,
			this,
			&ACityFlowGameMode::OnSimulationTimerExpired,
			SimulationDuration,
			false
		);

		OnPlanningPhaseEnd.Broadcast();
		break;
	}

	case ECityFlowGamePhase::Evaluation:
	{
		UVehicleManager* VM = GetVehicleManager();
		UScoringManager* SM = GetScoringManager();

		if (VM)
		{
			VM->StopSpawning();
		}

		if (SM)
		{
			SM->StopScoring();
		}

		OnSimulationPhaseEnd.Broadcast();
		break;
	}

	default:
		break;
	}
}

void ACityFlowGameMode::StartSimulationPhase()
{
	if (CurrentPhase != ECityFlowGamePhase::Planning)
	{
		return;
	}

	TransitionToPhase(ECityFlowGamePhase::Simulating);
}

void ACityFlowGameMode::EndSimulationPhase()
{
	if (CurrentPhase != ECityFlowGamePhase::Simulating)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SimulationTimerHandle);
	TransitionToPhase(ECityFlowGamePhase::Evaluation);
}

void ACityFlowGameMode::RestartPlanningPhase()
{
	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->ClearAllVehicles();
	}

	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::TriggerLSystemGrowth()
{
	if (CurrentPhase != ECityFlowGamePhase::Planning)
	{
		return;
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (!LSM)
	{
		return;
	}

	if (LSM->IsGenerating())
	{
		return;
	}

	LSM->SetBranchBudget(LSystemBudget);
	LSM->StartGenerate();
}

bool ACityFlowGameMode::CanPlaceRoad() const
{
	return CurrentPhase == ECityFlowGamePhase::Planning && PlayerBudget > 0;
}

bool ACityFlowGameMode::ConsumeRoadBudget(int32 Count)
{
	if (PlayerBudget < Count)
	{
		return false;
	}

	PlayerBudget -= Count;
	RemainingBudget -= Count;
	return true;
}

float ACityFlowGameMode::GetSimulationTimeRemaining() const
{
	if (CurrentPhase != ECityFlowGamePhase::Simulating)
	{
		return 0.0f;
	}

	if (GetWorldTimerManager().IsTimerActive(SimulationTimerHandle))
	{
		return GetWorldTimerManager().GetTimerRemaining(SimulationTimerHandle);
	}

	return 0.0f;
}

void ACityFlowGameMode::OnSimulationTimerExpired()
{
	EndSimulationPhase();
}

void ACityFlowGameMode::OnVehicleArrivedHandler(AVehicleActor* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}
}

bool ACityFlowGameMode::AreAllBuildingsConnected() const
{
	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (!LSM)
	{
		return false;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const TArray<FGridVector> BuildingCells = GM->GetCellsOfType(ECellType::Building);
	TSet<int32> SeenIDs;

	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GM->GetCell(CellPos);
		if (Cell.BuildingID != INDEX_NONE && !SeenIDs.Contains(Cell.BuildingID))
		{
			SeenIDs.Add(Cell.BuildingID);
		}
	}

	return true;
}

UGridManager* ACityFlowGameMode::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}

UVehicleManager* ACityFlowGameMode::GetVehicleManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UVehicleManager>();
}

UScoringManager* ACityFlowGameMode::GetScoringManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UScoringManager>();
}
