#pragma once

#include "CoreMinimal.h"
#include "ScoringTypes.generated.h"

/**
 * Immutable final report passed from ScoringManager to UI. Raw counters are kept
 * beside normalised scores so the player can understand how the total was formed.
 */
USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowScoreBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Final")
	int32 FinalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Final")
	float RawScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Final")
	float MapDifficultyMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Breakdown")
	float ConnectivityScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Breakdown")
	float TrafficOutcomeScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Breakdown")
	float TravelEfficiencyScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Breakdown")
	float BudgetEfficiencyScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Breakdown")
	float RuntimeScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 TotalBuildingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 ConnectedBuildingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 LargestConnectedBuildingComponent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	bool bAllConnected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 TotalRoadBudget = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 UsedBudget = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Planning")
	int32 EstimatedMinRoadNeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	int32 SpawnedVehicles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	int32 ArrivedVehicles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	int32 DeadVehicles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	int32 ActiveVehiclesAtEnd = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	float ArrivalRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	float DeathRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	float TotalTravelTimeOfArrivedVehicles = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	int32 TotalCellsTraversedByArrivedVehicles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	float AverageCellTravelTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoring|Traffic")
	float ElapsedSimulationTime = 0.0f;
};
