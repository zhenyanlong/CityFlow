#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "Grid/CityFlowGridTypes.h"
#include "Vehicle/Types/CityFlowVehicleTypes.h"
#include "CityFlowGameWidget.generated.h"

class UWidgetAnimation;
class UBuildingMarkerWidget;
class ABuilding;

UCLASS()
class CITYFLOW_API UCityFlowGameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// ---- BindWidget controls: matching Blueprint names are bound automatically ----

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> BuildingMarkerLayer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_VehicleAbilityAlert;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_VehicleAbilityAlert;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	TSubclassOf<class UScorePopupWidget> ScorePopupWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	FLinearColor PositivePopupColor = FLinearColor(0.15f, 1.0f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Score Popup")
	FLinearColor NegativePopupColor = FLinearColor(1.0f, 0.12f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	TSubclassOf<UBuildingMarkerWidget> BuildingMarkerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	bool bShowBuildingMarkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	bool bShowBuildingMarkersInPlanning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	bool bShowBuildingMarkersInSimulation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker")
	FVector BuildingMarkerWorldOffset = FVector(0.0f, 0.0f, 180.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Building Marker", meta = (ClampMin = "0.0"))
	float BuildingMarkerEdgePadding = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	float VehicleAbilityAlertDuration = 2.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	FLinearColor VehicleAbilityAlertColorA = FLinearColor(1.0f, 0.85f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	FLinearColor VehicleAbilityAlertColorB = FLinearColor(1.0f, 0.05f, 0.02f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	float VehicleAbilityAlertPulseFrequency = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	float VehicleAbilityAlertMinScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Vehicle Alert")
	float VehicleAbilityAlertMaxScale = 1.12f;

	// ---- Blueprint presentation hooks ----

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

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|UI")
	void OnVehicleAbilityAlert_BP(EVehicleAbilityAlertType AlertType, const FText& AlertText);

private:
	// ---- Button callbacks: UFUNCTION is required by AddDynamic ----
	UFUNCTION()
	void OnStartSimulationClicked();

	UFUNCTION()
	void OnRestartPlanningClicked();

	UFUNCTION()
	void OnTriggerLSystemClicked();

	// ---- Gameplay delegate callbacks ----
	UFUNCTION()
	void HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase);

	UFUNCTION()
	void HandleScoreChanged(int32 NewScore);

	UFUNCTION()
	void HandleScorePopupRequested(FVector WorldLocation, int32 DeltaScore);

	UFUNCTION()
	void HandleVehicleAbilityActivated(class AVehicleActor* Vehicle, EVehicleAbilityAlertType AlertType);

	UFUNCTION()
	void HandleLSystemStep(int32 RemainingBudget);

	UFUNCTION()
	void HandleLSystemFinished(bool bAllConnected);

	UFUNCTION()
	void HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell);

	// ---- Internal helpers ----
	void UpdatePhaseText(ECityFlowGamePhase Phase);
	void UpdateBudgetText();
	void UpdateButtonStates(ECityFlowGamePhase Phase);
	void ShowVehicleAbilityAlert(EVehicleAbilityAlertType AlertType);
	void HideVehicleAbilityAlert();
	void UpdateVehicleAbilityAlertFallback(float DeltaTime);
	void RequestBuildingMarkerRefresh();
	void RefreshBuildingMarkers();
	void ClearBuildingMarkers();
	void UpdateBuildingMarkers();
	TArray<ABuilding*> GetAllPlacedBuildings() const;
	bool ShouldShowBuildingMarkersForCurrentPhase() const;
	void SetBuildingMarkerPosition(UBuildingMarkerWidget* MarkerWidget, const FVector2D& Position) const;
	void StartCountdown();
	UFUNCTION()
	void TickCountdown();
	void StopCountdown();
	void UpdateCountdownText();

	class ACityFlowGameMode* GetCityFlowGameMode() const;
	class UScoringManager* GetScoringManager() const;
	int32 GetRemainingBudget() const;

	FTimerHandle CountdownTimerHandle;
	FTimerHandle VehicleAbilityAlertTimerHandle;
	UPROPERTY(Transient)
	TMap<TObjectPtr<ABuilding>, TObjectPtr<UBuildingMarkerWidget>> BuildingMarkers;
	int32 CountdownSeconds = 0;
	float VehicleAbilityAlertAge = 0.0f;
	bool bVehicleAbilityAlertActive = false;
	bool bVehicleAbilityAlertUsesNativeAnimation = false;
	bool bBuildingMarkersDirty = true;
};
