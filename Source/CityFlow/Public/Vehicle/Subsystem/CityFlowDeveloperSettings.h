#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CityFlowDeveloperSettings.generated.h"

class UVehicleDataAsset;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CityFlow Settings"))
class CITYFLOW_API UCityFlowDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Project"); }
#if WITH_EDITOR
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("CityFlow", "CityFlowSettingsSection", "CityFlow"); }
#endif

	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "10", ClampMax = "100"))
	int32 DefaultGridWidth = 24;

	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "10", ClampMax = "100"))
	int32 DefaultGridHeight = 24;

	UPROPERTY(Config, EditAnywhere, Category = "Grid", meta = (ClampMin = "50", ClampMax = "500"))
	float DefaultCellSize = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "10", ClampMax = "2000"))
	int32 TotalRoadBudget = 200;

	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "30", ClampMax = "600"))
	float SimulationDurationSeconds = 180.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.5", ClampMax = "30"))
	float VehicleSpawnInterval = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "5", ClampMax = "100"))
	int32 MaxVehicleCount = 40;

	/** Default desired in-flight population for non-difficulty matches such as the menu preview. */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "100"))
	int32 TargetActiveVehicleCount = 24;

	/** Maximum vehicles created on one refill pulse while below the target population. */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxSpawnBurstSize = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle", meta = (AllowedClasses = "/Script/CityFlow.VehicleDataAsset"))
	FSoftObjectPath DefaultVehicleDataAsset;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring", meta = (ClampMin = "1"))
	int32 ArrivalScore = 100;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring", meta = (ClampMin = "1"))
	int32 CongestionPenaltyPerSecond = 5;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring", meta = (ClampMin = "0"))
	int32 DeathPenalty = 50;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring", meta = (ClampMin = "0"))
	int32 FullConnectivityBonus = 500;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring", meta = (ClampMin = "2"))
	int32 CongestionThreshold = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "1.0"))
	float ReferenceBuildingCount = 8.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "0.1"))
	float ReferenceSpreadRatio = 6.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "0.01"))
	float TargetBudgetPressure = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "1.0"))
	float AcceptableCellTimeMultiplier = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "0.1"))
	float MinMapDifficultyMultiplier = 0.85f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Final", meta = (ClampMin = "0.1"))
	float MaxMapDifficultyMultiplier = 1.20f;

	UPROPERTY(Config, EditAnywhere, Category = "Scoring|Popup")
	bool bEnableScorePopups = true;

	UPROPERTY(Config, EditAnywhere, Category = "Buildings", meta = (ClampMin = "2"))
	int32 DefaultBuildingCount = 8;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugDrawPaths = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugDrawCongestion = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugDrawIntersections = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugVehicleAbilities = false;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDebugLogVehicles = false;

	UFUNCTION(BlueprintPure, Category = "CityFlow|Settings")
	static const UCityFlowDeveloperSettings* Get();
};
