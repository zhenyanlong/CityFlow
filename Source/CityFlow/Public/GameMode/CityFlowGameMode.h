#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowGameMode.generated.h"

UCLASS()
class CITYFLOW_API ACityFlowGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACityFlowGameMode();

	virtual void BeginPlay() override;

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

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Game")
	void InitializeDefaultScene();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class AGridPlaceableActor> OriginBuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class AGridPlaceableActor> DestinationBuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class ARoadTile> RoadTileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class AVehicleActor> VehicleClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Spawn")
	TSubclassOf<class UUserWidget> GameWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Budget")
	int32 TotalRoadBudget = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Budget")
	float LSystemBudgetShare = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Simulation")
	float SimulationDuration = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Buildings")
	int32 DefaultBuildingCount = 8;

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

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnGamePhaseChanged OnGamePhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnPlanningPhaseEnd OnPlanningPhaseEnd;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Events")
	FOnSimulationPhaseEnd OnSimulationPhaseEnd;

protected:
	void TransitionToPhase(ECityFlowGamePhase NewPhase);

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY()
	TObjectPtr<class UUserWidget> GameWidgetInstance;

private:
	void OnSimulationTimerExpired();
	void OnVehicleArrivedHandler(class AVehicleActor* Vehicle);
	bool AreAllBuildingsConnected() const;

	class UGridManager* GetGridManager() const;
	class UVehicleManager* GetVehicleManager() const;
	class UScoringManager* GetScoringManager() const;

	ECityFlowGamePhase CurrentPhase = ECityFlowGamePhase::None;
	int32 RemainingBudget = 0;
	int32 PlayerBudget = 0;
	int32 LSystemBudget = 0;
	float SimulationTimeRemaining = 0.0f;

	FTimerHandle SimulationTimerHandle;
};
