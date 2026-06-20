#pragma once

#include "CoreMinimal.h"
#include "CityFlowGameTypes.generated.h"

UENUM(BlueprintType)
enum class ECityFlowGamePhase : uint8
{
	None,
	Planning,
	Simulating,
	Evaluation
};

UENUM(BlueprintType)
enum class ECityFlowDrivingSide : uint8
{
	RightHand	UMETA(DisplayName = "右舵 (靠右行驶)"),
	LeftHand	UMETA(DisplayName = "左舵 (靠左行驶)")
};

UENUM(BlueprintType)
enum class ECityFlowDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

/** Complete per-match tuning applied after the player selects a Random Mode difficulty. */
USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowDifficultyProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty", meta = (ClampMin = "2"))
	int32 BuildingCount = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty", meta = (ClampMin = "0.1"))
	float VehicleSpawnInterval = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty", meta = (ClampMin = "30.0"))
	float SimulationDuration = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty", meta = (ClampMin = "1"))
	int32 RoadBudget = 230;

	/** Desired number of vehicles kept on the network when valid routes are available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Density", meta = (ClampMin = "1"))
	int32 TargetActiveVehicles = 26;

	/** Maximum successful spawns on one pulse while below the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Density", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SpawnBurstSize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic Density", meta = (ClampMin = "1"))
	int32 MaxActiveVehicles = 36;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePhaseChanged, ECityFlowGamePhase, OldPhase, ECityFlowGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlanningPhaseEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationPhaseEnd);
