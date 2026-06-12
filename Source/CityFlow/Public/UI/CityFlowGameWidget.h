#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "Grid/CityFlowGridTypes.h"
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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Countdown;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> PopupLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	TSubclassOf<class UScorePopupWidget> ScorePopupWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	FLinearColor PositivePopupColor = FLinearColor(0.15f, 1.0f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	FLinearColor NegativePopupColor = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);

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
	// ---- 按钮回调（必须是 UFUNCTION，否则 AddDynamic 绑定静默失败）----
	UFUNCTION()
	void OnStartSimulationClicked();

	UFUNCTION()
	void OnRestartPlanningClicked();

	UFUNCTION()
	void OnTriggerLSystemClicked();

	// ---- 委托回调 ----
	UFUNCTION()
	void HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase);

	UFUNCTION()
	void HandleScoreChanged(int32 NewScore);

	UFUNCTION()
	void HandleScorePopupRequested(FVector WorldLocation, int32 DeltaScore);

	UFUNCTION()
	void HandleLSystemStep(int32 RemainingBudget);

	UFUNCTION()
	void HandleLSystemFinished(bool bAllConnected);

	UFUNCTION()
	void HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell);

	// ---- 内部辅助 ----
	void UpdatePhaseText(ECityFlowGamePhase Phase);
	void UpdateBudgetText();
	void UpdateButtonStates(ECityFlowGamePhase Phase);
	void StartCountdown();
	UFUNCTION()
	void TickCountdown();
	void StopCountdown();
	void UpdateCountdownText();

	class ACityFlowGameMode* GetCityFlowGameMode() const;
	class UScoringManager* GetScoringManager() const;
	int32 GetRemainingBudget() const;

	FTimerHandle CountdownTimerHandle;
	int32 CountdownSeconds = 0;
};
