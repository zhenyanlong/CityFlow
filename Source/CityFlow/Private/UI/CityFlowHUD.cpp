#include "UI/CityFlowHUD.h"
#include "UI/CityFlowStartWidget.h"
#include "UI/CityFlowGameWidget.h"
#include "UI/CityFlowPauseWidget.h"
#include "UI/CityFlowEvaluationWidget.h"
#include "GameMode/CityFlowGameMode.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "Grid/GridManager.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Player/CityFlowPawn.h"
#include "Player/CityFlowPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
//  生命周期
// ============================================================================

void ACityFlowHUD::BeginPlay()
{
	Super::BeginPlay();

	// 监听 GameMode 阶段变化，自动切换 Widget
	if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnSimulationPhaseEnd.AddDynamic(this, &ACityFlowHUD::HandleSimulationEnded);
	}

	ShowStartWidget();
}

void ACityFlowHUD::HandleSimulationEnded()
{
	if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GM->IsCurrentMatchMenuPreview())
		{
			GM->StartAutomatedRandomMatch(true);
			return;
		}
	}

	ShowEvaluationWidget();
}

// ============================================================================
//  Widget 切换
// ============================================================================

void ACityFlowHUD::ShowStartWidget()
{
	// 清理已有 Widget
	if (GameWidget && GameWidget->IsInViewport())
		GameWidget->RemoveFromParent();
	if (EvaluationWidget && EvaluationWidget->IsInViewport())
		EvaluationWidget->RemoveFromParent();
	HidePauseOverlay();

	if (!StartWidget && StartWidgetClass)
		StartWidget = CreateWidget<UCityFlowStartWidget>(GetWorld(), StartWidgetClass);

	if (StartWidget && !StartWidget->IsInViewport())
	{
		StartWidget->AddToViewport();

		StartWidget->OnStartGameClicked.RemoveAll(this);
		StartWidget->OnStartGameClicked.AddDynamic(this, &ACityFlowHUD::HandleStartGameClicked);

		StartWidget->OnRandomModeClicked.RemoveAll(this);
		StartWidget->OnRandomModeClicked.AddDynamic(this, &ACityFlowHUD::HandleRandomModeClicked);
	}

	// 确保鼠标可见
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
		{
			CFPC->DisablePlacement();
		}
		if (ACityFlowPawn* CityFlowPawn = Cast<ACityFlowPawn>(PC->GetPawn()))
		{
			CityFlowPawn->SetMainMenuCameraYawRotationEnabled(true);
		}
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeUIOnly());
	}

	if (bEnableMainMenuPreviewMatch)
	{
		if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (!GM->IsCurrentMatchMenuPreview())
			{
				GM->StartAutomatedRandomMatch(true);
			}
		}
	}
}

void ACityFlowHUD::ShowGameWidget()
{
	// 隐藏 StartWidget
	if (StartWidget && StartWidget->IsInViewport())
		StartWidget->RemoveFromParent();

	if (!GameWidget && GameWidgetClass)
		GameWidget = CreateWidget<UCityFlowGameWidget>(GetWorld(), GameWidgetClass);

	if (GameWidget && !GameWidget->IsInViewport())
		GameWidget->AddToViewport();

	// GameMode 推迟初始化：首次进入 Planning 时创建场景
	if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GM->IsCurrentMatchMenuPreview())
		{
			GM->ReturnToMainMenu();
		}

		if (GM->GetCurrentPhase() == ECityFlowGamePhase::None)
		{
			GM->StartNewGame();
		}
	}

	// 恢复 Game + UI 输入模式（从 StartWidget 的 UIOnly 切换回来）
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACityFlowPawn* CityFlowPawn = Cast<ACityFlowPawn>(PC->GetPawn()))
		{
			CityFlowPawn->SetMainMenuCameraYawRotationEnabled(false);
		}
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
		{
			if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
			{
				if (GM->GetCurrentPhase() == ECityFlowGamePhase::Planning)
				{
					CFPC->EnablePlacement();
				}
			}
		}
	}
}

void ACityFlowHUD::ShowPauseOverlay()
{
	if (bPaused) return;

	if (!PauseWidget && PauseWidgetClass)
		PauseWidget = CreateWidget<UCityFlowPauseWidget>(GetWorld(), PauseWidgetClass);

	if (PauseWidget && !PauseWidget->IsInViewport())
	{
		PauseWidget->AddToViewport(100); // 叠在最上层

		PauseWidget->OnResumeClicked.RemoveAll(this);
		PauseWidget->OnResumeClicked.AddDynamic(this, &ACityFlowHUD::HandleResumeClicked);
		PauseWidget->OnReturnToMainClicked.RemoveAll(this);
		PauseWidget->OnReturnToMainClicked.AddDynamic(this, &ACityFlowHUD::HandleReturnToMainClicked);
	}

	bPaused = true;
	UGameplayStatics::SetGamePaused(GetWorld(), true);

	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeUIOnly());
	}
}

void ACityFlowHUD::HidePauseOverlay()
{
	if (!bPaused) return;

	if (PauseWidget && PauseWidget->IsInViewport())
		PauseWidget->RemoveFromParent();

	bPaused = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	if (APlayerController* PC = GetOwningPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
}

void ACityFlowHUD::ShowEvaluationWidget()
{
	if (!EvaluationWidget && EvaluationWidgetClass)
		EvaluationWidget = CreateWidget<UCityFlowEvaluationWidget>(GetWorld(), EvaluationWidgetClass);

	if (EvaluationWidget && !EvaluationWidget->IsInViewport())
	{
		EvaluationWidget->AddToViewport();

		// 委托已绑定过就不重复绑
		EvaluationWidget->OnBackToMainClicked.RemoveAll(this);
		EvaluationWidget->OnBackToMainClicked.AddDynamic(this, &ACityFlowHUD::HandleEvaluationReturn);

		EvaluationWidget->OnRestartClicked.RemoveAll(this);
		EvaluationWidget->OnRestartClicked.AddDynamic(this, &ACityFlowHUD::HandleRestartClicked);
	}

	// 从 ScoringManager 读取结算数据并填入 Widget
	UScoringManager* SM = GetWorld()->GetSubsystem<UScoringManager>();
	ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());

	if (SM && GM && EvaluationWidget)
	{
		EvaluationWidget->PopulateFromBreakdown(SM->GetScoreBreakdown());
	}

	if (GameWidget && GameWidget->IsInViewport())
		GameWidget->RemoveFromParent();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
		{
			CFPC->DisablePlacement();
		}
		if (ACityFlowPawn* CityFlowPawn = Cast<ACityFlowPawn>(PC->GetPawn()))
		{
			CityFlowPawn->SetMainMenuCameraYawRotationEnabled(false);
		}
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeUIOnly());
	}
}

// ============================================================================
//  暂停 / 恢复
// ============================================================================

void ACityFlowHUD::TogglePause()
{
	if (bPaused)
		HidePauseOverlay();
	else
		ShowPauseOverlay();
}

void ACityFlowHUD::HandleResumeClicked()
{
	HidePauseOverlay();
}

void ACityFlowHUD::HandleReturnToMainClicked()
{
	HidePauseOverlay();

	// 让 GameMode 清理并回到主菜单
	if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
		GM->ReturnToMainMenu();

	ShowStartWidget();
}

// ============================================================================
//  结算 → 主菜单
// ============================================================================

void ACityFlowHUD::ReturnToMainMenu()
{
	HandleEvaluationReturn();
}

void ACityFlowHUD::HandleEvaluationReturn()
{
	if (EvaluationWidget && EvaluationWidget->IsInViewport())
		EvaluationWidget->RemoveFromParent();

	if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
		GM->ReturnToMainMenu();

	ShowStartWidget();
}

// ============================================================================
//  StartWidget 按钮处理
// ============================================================================

void ACityFlowHUD::HandleStartGameClicked()
{
	ShowGameWidget();
}

void ACityFlowHUD::HandleRandomModeClicked()
{
	ShowGameWidgetRandom();
}

void ACityFlowHUD::HandleRestartClicked()
{
	ShowGameWidgetRandom();
}

void ACityFlowHUD::ShowGameWidgetRandom()
{
	// 隐藏 StartWidget
	if (StartWidget && StartWidget->IsInViewport())
		StartWidget->RemoveFromParent();
	if (EvaluationWidget && EvaluationWidget->IsInViewport())
		EvaluationWidget->RemoveFromParent();
	HidePauseOverlay();

	if (!GameWidget && GameWidgetClass)
		GameWidget = CreateWidget<UCityFlowGameWidget>(GetWorld(), GameWidgetClass);

	if (GameWidget && !GameWidget->IsInViewport())
		GameWidget->AddToViewport();

	ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode());
	if (!GM) return;

	GM->StartRandomPlanningGame();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACityFlowPawn* CityFlowPawn = Cast<ACityFlowPawn>(PC->GetPawn()))
		{
			CityFlowPawn->SetMainMenuCameraYawRotationEnabled(false);
		}
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
			CFPC->EnablePlacement();
	}
}
