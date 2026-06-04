#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowGameWidget.generated.h"

UCLASS()
class CITYFLOW_API UCityFlowGameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// ---- BindWidget 控件：蓝图放同名控件即自动绑定 ----

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_TriggerLSystem;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_StartSimulation;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_RestartPlanning;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Phase;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Budget;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Score;

	// ---- 蓝图事件回调 ----

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

private:
	void StartSimulation();
	void EndSimulation();
	void RestartPlanning();
	void TriggerLSystem();

	void UpdatePhaseText(ECityFlowGamePhase Phase);
	void UpdateBudgetText();
	void UpdateButtonStates(ECityFlowGamePhase Phase);

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
