#include "Vehicle/Subsystem/CityFlowCheatExtension.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "GameMode/CityFlowGameMode.h"
#include "Grid/GridManager.h"
#include "Grid/Building.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowCheat, Log, All);

void UCityFlowCheatExtension::CF_StartSimulation()
{
	ACityFlowGameMode* GM = GetGameMode();
	if (GM)
	{
		GM->StartSimulationPhase();
		UE_LOG(LogCityFlowCheat, Log, TEXT("Simulation started via cheat"));
	}
}

void UCityFlowCheatExtension::CF_EndSimulation()
{
	ACityFlowGameMode* GM = GetGameMode();
	if (GM)
	{
		GM->EndSimulationPhase();
		UE_LOG(LogCityFlowCheat, Log, TEXT("Simulation ended via cheat"));
	}
}

void UCityFlowCheatExtension::CF_RestartPlanning()
{
	ACityFlowGameMode* GM = GetGameMode();
	if (GM)
	{
		GM->RestartPlanningPhase();
		UE_LOG(LogCityFlowCheat, Log, TEXT("Planning restarted via cheat"));
	}
}

void UCityFlowCheatExtension::CF_TriggerLSystem()
{
	ACityFlowGameMode* GM = GetGameMode();
	if (GM)
	{
		GM->TriggerLSystemGrowth();
		UE_LOG(LogCityFlowCheat, Log, TEXT("L-System triggered via cheat"));
	}
}

void UCityFlowCheatExtension::CF_SpawnVehicle()
{
	UVehicleManager* VM = GetVehicleManager();
	UGridManager* GridM = GetGridManager();
	if (!VM || !GridM)
	{
		return;
	}

	const TArray<FGridVector> BuildingCells = GridM->GetCellsOfType(ECellType::Building);
	TArray<ABuilding*> Buildings;

	TSet<int32> Seen;
	for (const FGridVector& Pos : BuildingCells)
	{
		const FGridCell& Cell = GridM->GetCell(Pos);
		if (Cell.BuildingID == INDEX_NONE || Seen.Contains(Cell.BuildingID))
		{
			continue;
		}
		Seen.Add(Cell.BuildingID);

		ABuilding* Building = Cast<ABuilding>(Cell.RoadActor);
		if (Building)
		{
			Buildings.Add(Building);
		}
	}

	if (Buildings.Num() >= 2)
	{
		ABuilding* Origin = Buildings[0];
		ABuilding* Destination = Buildings[Buildings.Num() - 1];
		AVehicleActor* Vehicle = VM->SpawnVehicle(Origin, Destination);
		if (Vehicle)
		{
			UE_LOG(LogCityFlowCheat, Log, TEXT("Spawned vehicle %s from %s to %s"),
				*Vehicle->GetName(), *Origin->GetName(), *Destination->GetName());
		}
	}
	else
	{
		UE_LOG(LogCityFlowCheat, Warning, TEXT("Need at least 2 buildings, found %d"), Buildings.Num());
	}
}

void UCityFlowCheatExtension::CF_ClearVehicles()
{
	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->ClearAllVehicles();
		UE_LOG(LogCityFlowCheat, Log, TEXT("All vehicles cleared"));
	}
}

void UCityFlowCheatExtension::CF_TogglePathDebug()
{
	UCityFlowDeveloperSettings* Settings = GetMutableDefault<UCityFlowDeveloperSettings>();
	if (Settings)
	{
		Settings->bDebugDrawPaths = !Settings->bDebugDrawPaths;
		UE_LOG(LogCityFlowCheat, Log, TEXT("Path debug: %s"), Settings->bDebugDrawPaths ? TEXT("ON") : TEXT("OFF"));
	}
}

void UCityFlowCheatExtension::CF_ToggleIntersectionDebug()
{
	UCityFlowDeveloperSettings* Settings = GetMutableDefault<UCityFlowDeveloperSettings>();
	if (Settings)
	{
		Settings->bDebugDrawIntersections = !Settings->bDebugDrawIntersections;
		UE_LOG(LogCityFlowCheat, Log, TEXT("Intersection debug: %s"), Settings->bDebugDrawIntersections ? TEXT("ON") : TEXT("OFF"));
	}
}

void UCityFlowCheatExtension::CF_ToggleCongestionDebug()
{
	UCityFlowDeveloperSettings* Settings = GetMutableDefault<UCityFlowDeveloperSettings>();
	if (Settings)
	{
		Settings->bDebugDrawCongestion = !Settings->bDebugDrawCongestion;
		UE_LOG(LogCityFlowCheat, Log, TEXT("Congestion debug: %s"), Settings->bDebugDrawCongestion ? TEXT("ON") : TEXT("OFF"));
	}
}

void UCityFlowCheatExtension::CF_ToggleVehicleAbilityDebug()
{
	UCityFlowDeveloperSettings* Settings = GetMutableDefault<UCityFlowDeveloperSettings>();
	if (Settings)
	{
		Settings->bDebugVehicleAbilities = !Settings->bDebugVehicleAbilities;
		UE_LOG(LogCityFlowCheat, Log, TEXT("Vehicle ability debug: %s"), Settings->bDebugVehicleAbilities ? TEXT("ON") : TEXT("OFF"));
	}
}

void UCityFlowCheatExtension::CF_SetBudget(int32 Amount)
{
	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->SetRoadBudget(Amount);
		UE_LOG(LogCityFlowCheat, Log, TEXT("Road budget set to %d"), Amount);
	}
}

void UCityFlowCheatExtension::CF_AddBudget(int32 Amount)
{
	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->AddRoadBudget(Amount);
		UE_LOG(LogCityFlowCheat, Log, TEXT("Added %d to road budget. Remaining: %d"), Amount, GM->GetRemainingBudget());
	}
}

void UCityFlowCheatExtension::CF_ShowGridStats()
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const int32 RoadCount = GM->GetCellsOfType(ECellType::Road).Num();
	const int32 BuildingCount = GM->GetCellsOfType(ECellType::Building).Num();
	const int32 EmptyCount = GM->GetGridWidth() * GM->GetGridHeight() - RoadCount - BuildingCount;

	UE_LOG(LogCityFlowCheat, Log, TEXT("=== Grid Stats ==="));
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Size: %dx%d"), GM->GetGridWidth(), GM->GetGridHeight());
	UE_LOG(LogCityFlowCheat, Log, TEXT("  CellSize: %.1f"), GM->GetCellSize());
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Roads: %d | Buildings: %d | Empty: %d"), RoadCount, BuildingCount, EmptyCount);
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Budget remaining: %d"), GM->GetRemainingBudget());

	const TArray<FGridVector> Intersections = GM->GetCellsOfType(ECellType::Road).FilterByPredicate([GM](const FGridVector& P)
	{
		const uint8 Mask = GM->CalculateConnectedMask(P);
		int32 C = 0;
		if (Mask & 1) ++C;
		if (Mask & 2) ++C;
		if (Mask & 4) ++C;
		if (Mask & 8) ++C;
		return C >= 3;
	});
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Intersections: %d"), Intersections.Num());
}

void UCityFlowCheatExtension::CF_ShowVehicleStats()
{
	UVehicleManager* VM = GetVehicleManager();
	if (!VM)
	{
		return;
	}

	UE_LOG(LogCityFlowCheat, Log, TEXT("=== Vehicle Stats ==="));
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Active: %d | Arrived: %d"), VM->GetActiveVehicleCount(), VM->GetArrivedVehicleCount());

	const TArray<AVehicleActor*>& Active = VM->GetActiveVehicles();
	for (int32 i = 0; i < Active.Num(); ++i)
	{
		const AVehicleActor* V = Active[i];
		if (V)
		{
			const FString StateStr = StaticEnum<EVehicleMovementState>()->GetNameStringByValue(static_cast<int64>(V->GetMovementState()));
			const USplineComponent* Spline = V->GetPathSpline();
			const int32 SplinePts = Spline ? Spline->GetNumberOfSplinePoints() : 0;
			UE_LOG(LogCityFlowCheat, Log, TEXT("  [%d] %s | State: %s | SplinePts: %d | Speed: %.0f"),
				i, *V->GetName(), *StateStr, SplinePts, V->GetMoveSpeed());
		}
	}
}

void UCityFlowCheatExtension::CF_ShowScoreStats()
{
	UScoringManager* SM = GetScoringManager();
	if (!SM)
	{
		return;
	}

	const FCityFlowScoreBreakdown Breakdown = SM->GetScoreBreakdown();
	UE_LOG(LogCityFlowCheat, Log, TEXT("=== Score Stats ==="));
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Final: %d | Raw: %.1f | Multiplier: %.2f | Time: %.1fs"),
		Breakdown.FinalScore, Breakdown.RawScore, Breakdown.MapDifficultyMultiplier, Breakdown.ElapsedSimulationTime);
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Breakdown C/T/E/B/R: %.1f / %.1f / %.1f / %.1f / %.1f"),
		Breakdown.ConnectivityScore, Breakdown.TrafficOutcomeScore, Breakdown.TravelEfficiencyScore,
		Breakdown.BudgetEfficiencyScore, Breakdown.RuntimeScore);
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Planning: Buildings %d/%d | LargestComponent %d | Budget %d/%d | EstMin %d"),
		Breakdown.ConnectedBuildingCount, Breakdown.TotalBuildingCount, Breakdown.LargestConnectedBuildingComponent,
		Breakdown.UsedBudget, Breakdown.TotalRoadBudget, Breakdown.EstimatedMinRoadNeed);
	UE_LOG(LogCityFlowCheat, Log, TEXT("  Traffic: Spawned %d | Arrived %d | Dead %d | ActiveEnd %d | ArrivalRate %.2f | AvgCell %.2fs"),
		Breakdown.SpawnedVehicles, Breakdown.ArrivedVehicles, Breakdown.DeadVehicles, Breakdown.ActiveVehiclesAtEnd,
		Breakdown.ArrivalRate, Breakdown.AverageCellTravelTime);
}

void UCityFlowCheatExtension::CF_AddScore(int32 Amount)
{
	UE_LOG(LogCityFlowCheat, Log, TEXT("Score adjustment: %+d (not implemented in scoring manager directly)"), Amount);
}

void UCityFlowCheatExtension::CF_SetSimulationSpeed(float Speed)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetWorldSettings()->TimeDilation = Speed;
		UE_LOG(LogCityFlowCheat, Log, TEXT("Simulation speed set to %.2fx"), Speed);
	}
}

ACityFlowGameMode* UCityFlowCheatExtension::GetGameMode() const
{
	return Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());
}

UGridManager* UCityFlowCheatExtension::GetGridManager() const
{
	return GetWorld()->GetSubsystem<UGridManager>();
}

UVehicleManager* UCityFlowCheatExtension::GetVehicleManager() const
{
	return GetWorld()->GetSubsystem<UVehicleManager>();
}

UScoringManager* UCityFlowCheatExtension::GetScoringManager() const
{
	return GetWorld()->GetSubsystem<UScoringManager>();
}

ULSystemManager* UCityFlowCheatExtension::GetLSystemManager() const
{
	return GetWorld()->GetSubsystem<ULSystemManager>();
}
