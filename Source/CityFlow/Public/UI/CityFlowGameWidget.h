#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowGameWidget.generated.h"

UCLASS()
class CITYFLOW_API UCityFlowGameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void StartSimulation();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void EndSimulation();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void RestartPlanning();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void TriggerLSystem();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnPhaseChanged_BP(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnScoreChanged_BP(int32 NewScore);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnBudgetChanged_BP(int32 PlayerBudget, int32 LSystemBudget, int32 RemainingBudget);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnSimulationTick_BP(float TimeRemaining, int32 VehicleCount, int32 ArrivedCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnEvaluation_BP(int32 TotalScore, int32 Arrivals, int32 Penalty, bool bAllConnected);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnLSystemStep_BP(int32 RemainingBudget);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnLSystemFinished_BP(bool bAllConnected);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	ECityFlowGamePhase GetCurrentPhase() const;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	int32 GetTotalScore() const;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	int32 GetRemainingBudget() const;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	float GetSimulationTimeRemaining() const;

private:
	UFUNCTION()
	void HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase);

	UFUNCTION()
	void HandleScoreChanged(int32 NewScore);

	UFUNCTION()
	void HandleLSystemStep(int32 RemainingBudget);

	UFUNCTION()
	void HandleLSystemFinished(bool bAllConnected);

	UFUNCTION()
	void HandleSimulationEnd();

	class ACityFlowGameMode* GetCityFlowGameMode() const;
	class UScoringManager* GetScoringManager() const;
};
