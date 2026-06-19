#include "UI/CityFlowGameWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Grid/Building.h"
#include "Grid/GridManager.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "UI/BuildingMarkerWidget.h"
#include "UI/ScorePopupWidget.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Player/CityFlowPlayerController.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "CityFlowGameWidget"

// ============================================================================
//  生命周期
// ============================================================================

void UCityFlowGameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Txt_VehicleAbilityAlert)
	{
		Txt_VehicleAbilityAlert->SetVisibility(ESlateVisibility::Collapsed);
		Txt_VehicleAbilityAlert->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	// 绑定按钮（回调必须是 UFUNCTION，否则 AddDynamic 静默失败）
	if (Btn_TriggerLSystem)
		Btn_TriggerLSystem->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnTriggerLSystemClicked);
	if (Btn_StartSimulation)
		Btn_StartSimulation->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnStartSimulationClicked);
	if (Btn_RestartPlanning)
		Btn_RestartPlanning->OnClicked.AddDynamic(this, &UCityFlowGameWidget::OnRestartPlanningClicked);

	// 绑定 GameMode / Scoring / LSystem 委托
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->OnGamePhaseChanged.AddDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);

	if (UScoringManager* SM = GetScoringManager())
	{
		SM->OnScoreChanged.AddDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
		SM->OnScorePopupRequested.AddDynamic(this, &UCityFlowGameWidget::HandleScorePopupRequested);
	}

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.AddDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	if (UVehicleManager* VM = GetWorld()->GetSubsystem<UVehicleManager>())
	{
		VM->OnVehicleAbilityActivated.AddDynamic(this, &UCityFlowGameWidget::HandleVehicleAbilityActivated);
	}

	// 监听网格变化，随时刷新预算显示
	if (UGridManager* GridMgr = GetWorld()->GetSubsystem<UGridManager>())
	{
		GridMgr->OnCellChanged.AddDynamic(this, &UCityFlowGameWidget::HandleCellChanged);
	}

	// 初始 UI 状态
	UpdateButtonStates(GetCityFlowGameMode() ? GetCityFlowGameMode()->GetCurrentPhase() : ECityFlowGamePhase::None);
	RequestBuildingMarkerRefresh();
}

void UCityFlowGameWidget::NativeDestruct()
{
	// 解绑按钮
	if (Btn_TriggerLSystem)     Btn_TriggerLSystem->OnClicked.RemoveAll(this);
	if (Btn_StartSimulation)    Btn_StartSimulation->OnClicked.RemoveAll(this);
	if (Btn_RestartPlanning)    Btn_RestartPlanning->OnClicked.RemoveAll(this);

	// 解绑委托
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->OnGamePhaseChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleGamePhaseChanged);

	if (UScoringManager* SM = GetScoringManager())
	{
		SM->OnScoreChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleScoreChanged);
		SM->OnScorePopupRequested.RemoveDynamic(this, &UCityFlowGameWidget::HandleScorePopupRequested);
	}

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationStep.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemStep);
		LSM->OnGenerationFinished.RemoveDynamic(this, &UCityFlowGameWidget::HandleLSystemFinished);
	}

	if (UVehicleManager* VM = GetWorld()->GetSubsystem<UVehicleManager>())
	{
		VM->OnVehicleAbilityActivated.RemoveDynamic(this, &UCityFlowGameWidget::HandleVehicleAbilityActivated);
	}

	if (UGridManager* GridMgr = GetWorld()->GetSubsystem<UGridManager>())
		GridMgr->OnCellChanged.RemoveDynamic(this, &UCityFlowGameWidget::HandleCellChanged);

	ClearBuildingMarkers();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VehicleAbilityAlertTimerHandle);
	}

	Super::NativeDestruct();
}

void UCityFlowGameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateVehicleAbilityAlertFallback(InDeltaTime);

	if (bBuildingMarkersDirty)
	{
		RefreshBuildingMarkers();
	}

	UpdateBuildingMarkers();
}

// ============================================================================
//  按钮回调
// ============================================================================

void UCityFlowGameWidget::OnStartSimulationClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->StartSimulationPhase();

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
		PC->DisablePlacement();
}

void UCityFlowGameWidget::OnRestartPlanningClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->RestartPlanningPhase();

	if (ACityFlowPlayerController* PC = GetOwningPlayer<ACityFlowPlayerController>())
		PC->EnablePlacement();
}

void UCityFlowGameWidget::OnTriggerLSystemClicked()
{
	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		GM->TriggerLSystemGrowth();
}

// ============================================================================
//  委托回调
// ============================================================================

void UCityFlowGameWidget::HandleGamePhaseChanged(ECityFlowGamePhase OldPhase, ECityFlowGamePhase NewPhase)
{
	UpdatePhaseText(NewPhase);
	UpdateBudgetText();
	UpdateButtonStates(NewPhase);
	RequestBuildingMarkerRefresh();

	if (NewPhase == ECityFlowGamePhase::Simulating)
		StartCountdown();
	else
		StopCountdown();

	OnPhaseChanged_BP(OldPhase, NewPhase);

	if (ACityFlowGameMode* GM = GetCityFlowGameMode())
		OnBudgetChanged_BP(GM->GetPlayerBudget(), GM->GetLSystemBudget(), GM->GetRemainingBudget());
}

void UCityFlowGameWidget::HandleScoreChanged(int32 NewScore)
{
	if (Txt_Score)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Score"), FText::AsNumber(NewScore));
		Txt_Score->SetText(FText::Format(LOCTEXT("ScoreFormat", "Score: {Score}"), Args));
	}

	OnScoreChanged_BP(NewScore);
}

void UCityFlowGameWidget::HandleScorePopupRequested(FVector WorldLocation, int32 DeltaScore)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	TSubclassOf<UScorePopupWidget> PopupClass = ScorePopupWidgetClass;
	if (!PopupClass)
	{
		PopupClass = UScorePopupWidget::StaticClass();
	}

	UScorePopupWidget* Popup = CreateWidget<UScorePopupWidget>(PC, PopupClass);
	if (!Popup)
	{
		return;
	}

	if (PopupLayer)
	{
		UCanvasPanelSlot* CanvasSlot = PopupLayer->AddChildToCanvas(Popup);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
	}
	else
	{
		Popup->AddToViewport(20);
	}

	const FLinearColor PopupColor = DeltaScore >= 0 ? PositivePopupColor : NegativePopupColor;
	Popup->InitializeScreenPopup(PC, WorldLocation, DeltaScore, PopupColor);
}

void UCityFlowGameWidget::HandleVehicleAbilityActivated(AVehicleActor* Vehicle, EVehicleAbilityAlertType AlertType)
{
	ShowVehicleAbilityAlert(AlertType);
}

void UCityFlowGameWidget::HandleLSystemStep(int32 RemainingBudget)
{
	if (Txt_Budget)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Budget"), FText::AsNumber(RemainingBudget));
		Txt_Budget->SetText(FText::Format(LOCTEXT("BudgetFormat", "Budget: {Budget}"), Args));
	}

	OnLSystemStep_BP(RemainingBudget);
}

void UCityFlowGameWidget::HandleLSystemFinished(bool bAllConnected)
{
	UpdateBudgetText();
	OnLSystemFinished_BP(bAllConnected);
}

// ============================================================================
//  UI 更新辅助
// ============================================================================

void UCityFlowGameWidget::UpdatePhaseText(ECityFlowGamePhase Phase)
{
	if (!Txt_Phase) return;

	FText PhaseText = LOCTEXT("UnknownPhase", "Unknown");
	switch (Phase)
	{
	case ECityFlowGamePhase::Planning:   PhaseText = LOCTEXT("PlanningPhase", "Planning"); break;
	case ECityFlowGamePhase::Simulating: PhaseText = LOCTEXT("SimulatingPhase", "Simulating"); break;
	case ECityFlowGamePhase::Evaluation: PhaseText = LOCTEXT("EvaluationPhase", "Evaluation"); break;
	default: break;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Phase"), PhaseText);
	Txt_Phase->SetText(FText::Format(LOCTEXT("PhaseFormat", "Phase: {Phase}"), Args));
}

void UCityFlowGameWidget::UpdateBudgetText()
{
	if (!Txt_Budget) return;

	const int32 Remaining = GetRemainingBudget();
	FFormatNamedArguments Args;
	Args.Add(TEXT("Budget"), FText::AsNumber(Remaining));
	Txt_Budget->SetText(FText::Format(LOCTEXT("BudgetFormat", "Budget: {Budget}"), Args));
}

void UCityFlowGameWidget::UpdateButtonStates(ECityFlowGamePhase Phase)
{
	using EVis = ESlateVisibility;
	const bool bPlanning = (Phase == ECityFlowGamePhase::Planning);
	const bool bSimulating = (Phase == ECityFlowGamePhase::Simulating);

	if (Btn_TriggerLSystem)   Btn_TriggerLSystem->SetVisibility(bPlanning ? EVis::Visible : EVis::Collapsed);
	if (Btn_StartSimulation)  Btn_StartSimulation->SetVisibility(bPlanning ? EVis::Visible : EVis::Collapsed);
	if (Btn_RestartPlanning)  Btn_RestartPlanning->SetVisibility(bSimulating ? EVis::Visible : EVis::Collapsed);
}

void UCityFlowGameWidget::ShowVehicleAbilityAlert(EVehicleAbilityAlertType AlertType)
{
	if (!Txt_VehicleAbilityAlert)
	{
		return;
	}

	const FText AlertText = AlertType == EVehicleAbilityAlertType::Rampage
		? LOCTEXT("RampageVehicleAlert", "A vehicle has gone out of control!!!")
		: LOCTEXT("TeleportVehicleAlert", "A vehicle teleported!!!");

	Txt_VehicleAbilityAlert->SetText(AlertText);
	Txt_VehicleAbilityAlert->SetVisibility(ESlateVisibility::HitTestInvisible);
	Txt_VehicleAbilityAlert->SetColorAndOpacity(FSlateColor(VehicleAbilityAlertColorA));
	Txt_VehicleAbilityAlert->SetRenderOpacity(1.0f);
	Txt_VehicleAbilityAlert->SetRenderScale(FVector2D(VehicleAbilityAlertMaxScale, VehicleAbilityAlertMaxScale));

	VehicleAbilityAlertAge = 0.0f;
	bVehicleAbilityAlertActive = true;
	bVehicleAbilityAlertUsesNativeAnimation = Anim_VehicleAbilityAlert != nullptr;

	if (Anim_VehicleAbilityAlert)
	{
		StopAnimation(Anim_VehicleAbilityAlert);
		PlayAnimation(Anim_VehicleAbilityAlert, 0.0f, 1);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VehicleAbilityAlertTimerHandle);
		World->GetTimerManager().SetTimer(
			VehicleAbilityAlertTimerHandle,
			this,
			&UCityFlowGameWidget::HideVehicleAbilityAlert,
			VehicleAbilityAlertDuration,
			false);
	}

	OnVehicleAbilityAlert_BP(AlertType, AlertText);
}

void UCityFlowGameWidget::HideVehicleAbilityAlert()
{
	bVehicleAbilityAlertActive = false;
	bVehicleAbilityAlertUsesNativeAnimation = false;
	VehicleAbilityAlertAge = 0.0f;

	if (Txt_VehicleAbilityAlert)
	{
		Txt_VehicleAbilityAlert->SetVisibility(ESlateVisibility::Collapsed);
		Txt_VehicleAbilityAlert->SetRenderOpacity(0.0f);
		Txt_VehicleAbilityAlert->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}

void UCityFlowGameWidget::UpdateVehicleAbilityAlertFallback(float DeltaTime)
{
	if (!bVehicleAbilityAlertActive || bVehicleAbilityAlertUsesNativeAnimation || !Txt_VehicleAbilityAlert)
	{
		return;
	}

	VehicleAbilityAlertAge += DeltaTime;
	const float Duration = FMath::Max(VehicleAbilityAlertDuration, KINDA_SMALL_NUMBER);
	const float NormalizedAge = FMath::Clamp(VehicleAbilityAlertAge / Duration, 0.0f, 1.0f);
	const float PulseAlpha = (FMath::Sin(VehicleAbilityAlertAge * VehicleAbilityAlertPulseFrequency * 2.0f * PI) + 1.0f) * 0.5f;
	const float Scale = FMath::Lerp(VehicleAbilityAlertMinScale, VehicleAbilityAlertMaxScale, PulseAlpha);

	FLinearColor AlertColor = FMath::Lerp(VehicleAbilityAlertColorA, VehicleAbilityAlertColorB, PulseAlpha);
	AlertColor.A = 1.0f - FMath::SmoothStep(0.75f, 1.0f, NormalizedAge);

	Txt_VehicleAbilityAlert->SetColorAndOpacity(FSlateColor(AlertColor));
	Txt_VehicleAbilityAlert->SetRenderOpacity(AlertColor.A);
	Txt_VehicleAbilityAlert->SetRenderScale(FVector2D(Scale, Scale));
}

// ============================================================================
//  网格变更 → 刷新预算
// ============================================================================

void UCityFlowGameWidget::HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell)
{
	UpdateBudgetText();
	RequestBuildingMarkerRefresh();
}

// ============================================================================
//  辅助
// ============================================================================

void UCityFlowGameWidget::RequestBuildingMarkerRefresh()
{
	bBuildingMarkersDirty = true;
}

void UCityFlowGameWidget::RefreshBuildingMarkers()
{
	bBuildingMarkersDirty = false;

	if (!ShouldShowBuildingMarkersForCurrentPhase())
	{
		ClearBuildingMarkers();
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	const TArray<ABuilding*> Buildings = GetAllPlacedBuildings();
	TSet<TObjectPtr<ABuilding>> ActiveBuildings;
	for (ABuilding* Building : Buildings)
	{
		if (IsValid(Building))
		{
			ActiveBuildings.Add(Building);
		}
	}

	TArray<TObjectPtr<ABuilding>> StaleBuildings;
	for (const TPair<TObjectPtr<ABuilding>, TObjectPtr<UBuildingMarkerWidget>>& Pair : BuildingMarkers)
	{
		ABuilding* Building = Pair.Key.Get();
		if (!IsValid(Building) || !ActiveBuildings.Contains(Pair.Key))
		{
			StaleBuildings.Add(Pair.Key);
		}
	}

	for (const TObjectPtr<ABuilding>& Building : StaleBuildings)
	{
		if (TObjectPtr<UBuildingMarkerWidget>* MarkerPtr = BuildingMarkers.Find(Building))
		{
			if (UBuildingMarkerWidget* Marker = MarkerPtr->Get())
			{
				Marker->RemoveFromParent();
			}
		}
		BuildingMarkers.Remove(Building);
	}

	TSubclassOf<UBuildingMarkerWidget> MarkerClass = BuildingMarkerWidgetClass;
	if (!MarkerClass)
	{
		MarkerClass = UBuildingMarkerWidget::StaticClass();
	}

	for (ABuilding* Building : Buildings)
	{
		if (!IsValid(Building) || BuildingMarkers.Contains(Building))
		{
			continue;
		}

		UBuildingMarkerWidget* Marker = CreateWidget<UBuildingMarkerWidget>(PC, MarkerClass);
		if (!Marker)
		{
			continue;
		}

		if (BuildingMarkerLayer)
		{
			if (UCanvasPanelSlot* CanvasSlot = BuildingMarkerLayer->AddChildToCanvas(Marker))
			{
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}
		}
		else
		{
			Marker->AddToViewport(15);
			Marker->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		}

		Marker->InitializeMarker(Building);
		BuildingMarkers.Add(Building, Marker);
	}
}

void UCityFlowGameWidget::ClearBuildingMarkers()
{
	for (TPair<TObjectPtr<ABuilding>, TObjectPtr<UBuildingMarkerWidget>>& Pair : BuildingMarkers)
	{
		if (UBuildingMarkerWidget* Marker = Pair.Value.Get())
		{
			Marker->RemoveFromParent();
		}
	}
	BuildingMarkers.Empty();
}

void UCityFlowGameWidget::UpdateBuildingMarkers()
{
	if (BuildingMarkers.IsEmpty())
	{
		return;
	}

	if (!ShouldShowBuildingMarkersForCurrentPhase())
	{
		ClearBuildingMarkers();
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;
	if (ViewportSize.X <= 1.0f || ViewportSize.Y <= 1.0f)
	{
		return;
	}

	const FVector2D ViewportCenter = ViewportSize * 0.5f;
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
	const FVector CameraForward = CameraRotation.Vector();

	bool bFoundInvalidBuilding = false;
	for (const TPair<TObjectPtr<ABuilding>, TObjectPtr<UBuildingMarkerWidget>>& Pair : BuildingMarkers)
	{
		ABuilding* Building = Pair.Key.Get();
		UBuildingMarkerWidget* Marker = Pair.Value.Get();
		if (!IsValid(Building) || !Marker)
		{
			bFoundInvalidBuilding = true;
			continue;
		}

		const FVector MarkerWorldLocation = Building->GetActorLocation() + BuildingMarkerWorldOffset;
		FVector2D ProjectedPosition = FVector2D::ZeroVector;
		const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PC,
			MarkerWorldLocation,
			ProjectedPosition,
			true);

		const FVector ToTarget = MarkerWorldLocation - CameraLocation;
		const bool bBehindCamera = FVector::DotProduct(CameraForward, ToTarget) <= 0.0f;
		const bool bInsideViewport =
			bProjected &&
			!bBehindCamera &&
			ProjectedPosition.X >= 0.0f &&
			ProjectedPosition.X <= ViewportSize.X &&
			ProjectedPosition.Y >= 0.0f &&
			ProjectedPosition.Y <= ViewportSize.Y;

		if (bInsideViewport)
		{
			Marker->SetMarkerScreenState(true, 0.0f);
			SetBuildingMarkerPosition(Marker, ProjectedPosition);
			continue;
		}

		FVector2D Direction = FVector2D::ZeroVector;
		if (bProjected && !bBehindCamera)
		{
			Direction = ProjectedPosition - ViewportCenter;
		}
		else
		{
			const FVector CameraLocalTarget = CameraRotation.UnrotateVector(ToTarget);
			Direction = FVector2D(CameraLocalTarget.Y, -CameraLocalTarget.Z);
		}

		if (!Direction.Normalize())
		{
			Direction = FVector2D(0.0f, 1.0f);
		}

		const FVector2D HalfBounds(
			FMath::Max(ViewportCenter.X - BuildingMarkerEdgePadding, 1.0f),
			FMath::Max(ViewportCenter.Y - BuildingMarkerEdgePadding, 1.0f));

		const float TimeToX = !FMath::IsNearlyZero(Direction.X)
			? HalfBounds.X / FMath::Abs(Direction.X)
			: TNumericLimits<float>::Max();
		const float TimeToY = !FMath::IsNearlyZero(Direction.Y)
			? HalfBounds.Y / FMath::Abs(Direction.Y)
			: TNumericLimits<float>::Max();
		const float EdgeTime = FMath::Min(TimeToX, TimeToY);
		const FVector2D EdgePosition = ViewportCenter + Direction * EdgeTime;
		const float DirectionAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));

		Marker->SetMarkerScreenState(false, DirectionAngleDegrees);
		SetBuildingMarkerPosition(Marker, EdgePosition);
	}

	if (bFoundInvalidBuilding)
	{
		RequestBuildingMarkerRefresh();
	}
}

TArray<ABuilding*> UCityFlowGameWidget::GetAllPlacedBuildings() const
{
	TArray<ABuilding*> Result;
	UGridManager* GridMgr = GetWorld() ? GetWorld()->GetSubsystem<UGridManager>() : nullptr;
	if (!GridMgr || !GridMgr->IsGridInitialized())
	{
		return Result;
	}

	const TArray<FGridVector> BuildingCells = GridMgr->GetCellsOfType(ECellType::Building);
	TSet<int32> SeenBuildingIds;
	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GridMgr->GetCell(CellPos);
		if (Cell.BuildingID == INDEX_NONE || SeenBuildingIds.Contains(Cell.BuildingID))
		{
			continue;
		}

		SeenBuildingIds.Add(Cell.BuildingID);
		if (ABuilding* Building = Cast<ABuilding>(Cell.RoadActor))
		{
			Result.Add(Building);
		}
	}

	return Result;
}

bool UCityFlowGameWidget::ShouldShowBuildingMarkersForCurrentPhase() const
{
	if (!bShowBuildingMarkers)
	{
		return false;
	}

	const ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (!GM)
	{
		return false;
	}

	const ECityFlowGamePhase Phase = GM->GetCurrentPhase();
	return (Phase == ECityFlowGamePhase::Planning && bShowBuildingMarkersInPlanning) ||
		(Phase == ECityFlowGamePhase::Simulating && bShowBuildingMarkersInSimulation);
}

void UCityFlowGameWidget::SetBuildingMarkerPosition(UBuildingMarkerWidget* MarkerWidget, const FVector2D& Position) const
{
	if (!MarkerWidget)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
	{
		CanvasSlot->SetPosition(Position);
	}
	else
	{
		MarkerWidget->SetPositionInViewport(Position, false);
	}
}

ACityFlowGameMode* UCityFlowGameWidget::GetCityFlowGameMode() const
{
	return Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());
}

UScoringManager* UCityFlowGameWidget::GetScoringManager() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<UScoringManager>() : nullptr;
}

int32 UCityFlowGameWidget::GetRemainingBudget() const
{
	if (UGridManager* GM = GetWorld()->GetSubsystem<UGridManager>())
		return GM->GetRemainingBudget();
	return 0;
}

// ============================================================================
//  倒计时
// ============================================================================

void UCityFlowGameWidget::StartCountdown()
{
	ACityFlowGameMode* GM = GetCityFlowGameMode();
	if (!GM) return;

	CountdownSeconds = FMath::CeilToInt(GM->SimulationDuration);
	UpdateCountdownText();

	GetWorld()->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&UCityFlowGameWidget::TickCountdown,
		1.0f,
		true
	);
}

void UCityFlowGameWidget::TickCountdown()
{
	--CountdownSeconds;
	UpdateCountdownText();

	if (CountdownSeconds <= 0)
	{
		StopCountdown();
	}
}

void UCityFlowGameWidget::StopCountdown()
{
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
}

void UCityFlowGameWidget::UpdateCountdownText()
{
	if (!Txt_Countdown) return;

	const int32 Mins = CountdownSeconds / 60;
	const int32 Secs = CountdownSeconds % 60;
	FNumberFormattingOptions TwoDigitOptions;
	TwoDigitOptions.MinimumIntegralDigits = 2;
	TwoDigitOptions.MaximumIntegralDigits = 2;
	FFormatNamedArguments Args;
	Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, &TwoDigitOptions));
	Args.Add(TEXT("Seconds"), FText::AsNumber(Secs, &TwoDigitOptions));
	Txt_Countdown->SetText(FText::Format(LOCTEXT("CountdownFormat", "{Minutes}:{Seconds}"), Args));
}

#undef LOCTEXT_NAMESPACE
