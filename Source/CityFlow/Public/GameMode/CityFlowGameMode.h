#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowGameMode.generated.h"

class UBuildingDataAsset;
class UVehicleDataAsset;

UCLASS()
class CITYFLOW_API ACityFlowGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACityFlowGameMode();

	virtual void BeginPlay() override;

	/** 玩家点击 "开始游戏" 后调用，创建场景并进入 Planning */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void StartNewGame();

	/** 使用随机参数创建一局自动对局：生成场景、生成道路，并在道路生成完成后进入模拟 */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void StartAutomatedRandomMatch(bool bAsMenuPreview);

	/** 使用随机参数创建一局玩家可操作的规划开局：只生成景观和建筑，停留在 Planning */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void StartRandomPlanningGame();

	/** Starts Random Mode with an explicit, Blueprint-configurable difficulty profile. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void StartRandomPlanningGameWithDifficulty(ECityFlowDifficulty Difficulty);

	/** 回到主菜单 —— 清理所有 Actor 和状态 */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void StartSimulationPhase();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void EndSimulationPhase();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void RestartPlanningPhase();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void TriggerLSystemGrowth();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	bool CanPlaceRoad() const;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	bool ConsumeRoadBudget(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	int32 GetRemainingBudget() const { return RemainingBudget; }

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	int32 GetPlayerBudget() const { return PlayerBudget; }

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	int32 GetLSystemBudget() const { return LSystemBudget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Game")
	ECityFlowGamePhase GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Game")
	float GetSimulationTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "CityFlow|Game")
	bool IsCurrentMatchMenuPreview() const { return bCurrentMatchIsMenuPreview; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Difficulty")
	FCityFlowDifficultyProfile GetDifficultyProfile(ECityFlowDifficulty Difficulty) const;

	UFUNCTION(BlueprintPure, Category = "CityFlow|Difficulty")
	ECityFlowDifficulty GetActiveDifficulty() const { return ActiveDifficulty; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Difficulty")
	float GetActiveSimulationDuration() const { return ActiveSimulationDuration; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Difficulty")
	float GetActiveVehicleSpawnInterval() const { return ActiveVehicleSpawnInterval; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|Difficulty")
	int32 GetActiveVehicleSpawnBurstSize() const { return ActiveVehicleSpawnBurstSize; }

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void InitializeDefaultScene();

	/** 建筑生成数据资产 —— 配置可生成的建筑类型及权重。设置后优先使用，否则回退到 OriginBuildingClass/DestinationBuildingClass */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TObjectPtr<UBuildingDataAsset> BuildingDataAsset;

	/** 车辆生成数据资产 —— 配置可生成的车辆类型及权重。设置后优先使用，否则回退到 DeveloperSettings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TObjectPtr<UVehicleDataAsset> VehicleDataAsset;

	/** @deprecated 回退用：当 BuildingDataAsset 未设置时使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class AGridPlaceableActor> OriginBuildingClass;

	/** @deprecated 回退用：当 BuildingDataAsset 未设置时使用 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class AGridPlaceableActor> DestinationBuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class ARoadTile> RoadTileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Budget")
	int32 TotalRoadBudget = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Budget")
	float LSystemBudgetShare = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Simulation")
	float SimulationDuration = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Buildings")
	int32 DefaultBuildingCount = 8;

	/** Easy gives generous road budget per building and lighter traffic pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Difficulty")
	FCityFlowDifficultyProfile EasyDifficultyProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Difficulty")
	FCityFlowDifficultyProfile MediumDifficultyProfile;

	/** Hard increases map complexity and traffic while reducing budget per building. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Difficulty")
	FCityFlowDifficultyProfile HardDifficultyProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Grid")
	int32 DefaultGridWidth = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Grid")
	int32 DefaultGridHeight = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Grid")
	float DefaultCellSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Vehicle")
	ECityFlowDrivingSide DrivingSide = ECityFlowDrivingSide::RightHand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Vehicle", meta = (ClampMin = "0.0", ClampMax = "0.45"))
	float LaneOffsetFactor = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Auto Match")
	bool bRandomizeAutoMatchParameters = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Auto Match")
	FIntPoint AutoMatchGridWidthRange = FIntPoint(20, 28);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Auto Match")
	FIntPoint AutoMatchGridHeightRange = FIntPoint(20, 28);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Auto Match")
	FIntPoint AutoMatchBuildingCountRange = FIntPoint(6, 12);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Auto Match")
	FIntPoint AutoMatchRoadBudgetRange = FIntPoint(120, 260);

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnGamePhaseChanged OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnPlanningPhaseEnd OnPlanningPhaseEnd;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnSimulationPhaseEnd OnSimulationPhaseEnd;

protected:
	void TransitionToPhase(ECityFlowGamePhase NewPhase);

	virtual void Tick(float DeltaSeconds) override;

private:
	void OnSimulationTimerExpired();
	void OnVehicleArrivedHandler(class AVehicleActor* Vehicle);
	UFUNCTION()
	void HandleAutoLSystemFinished(bool bAllConnected);

	void ResetRuntimeScene();
	void ResetActiveMatchSettings();
	void ApplyDifficultyProfile(ECityFlowDifficulty Difficulty);
	void PickRandomSceneParameters(int32& OutGridWidth, int32& OutGridHeight, int32& OutBuildingCount, int32& OutRoadBudget, int32& OutRandomSeed) const;
	void InitializeScene(int32 GridWidth, int32 GridHeight, float CellSize, int32 BuildingCount, int32 RoadBudget, int32 RandomSeed);
	void ConfigureLSystemForActiveScene();
	void StartAutoRoadGenerationOrSimulation();
	int32 GetActiveTotalRoadBudget() const;
	bool AreAllBuildingsConnected() const;

	class UGridManager* GetGridManager() const;
	class UVehicleManager* GetVehicleManager() const;
	class UScoringManager* GetScoringManager() const;

	ECityFlowGamePhase CurrentPhase = ECityFlowGamePhase::None;
	int32 ActiveTotalRoadBudget = 0;
	int32 RemainingBudget = 0;
	int32 PlayerBudget = 0;
	int32 LSystemBudget = 0;
	float SimulationTimeRemaining = 0.0f;
	float ActiveSimulationDuration = 180.0f;
	float ActiveVehicleSpawnInterval = 0.65f;
	int32 ActiveVehicleTargetCount = 26;
	int32 ActiveVehicleSpawnBurstSize = 3;
	int32 ActiveMaxVehicleCount = 36;
	ECityFlowDifficulty ActiveDifficulty = ECityFlowDifficulty::Medium;
	bool bCurrentMatchIsMenuPreview = false;
	bool bAutoStartSimulationAfterLSystem = false;

	FTimerHandle SimulationTimerHandle;
};
