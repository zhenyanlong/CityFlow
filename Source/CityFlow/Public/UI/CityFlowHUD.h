#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CityFlowHUD.generated.h"

class UCityFlowStartWidget;
class UCityFlowGameWidget;
class UCityFlowPauseWidget;
class UCityFlowEvaluationWidget;

/**
 * HUD —— 管理完整 Widget 生命周期：
 *   StartWidget → GameWidget ↔ PauseWidget → EvaluationWidget → (轮回)
 *
 *   BeginPlay 显示 StartWidget；点击 "开始游戏" 进入 Planning；
 *   Esc 切换暂停菜单；结算后可返回主菜单。
 */
UCLASS()
class CITYFLOW_API ACityFlowHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/** 切换暂停 / 恢复（由 GameMode 的 Esc 输入调用） */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void TogglePause();

	/** 当前是否已暂停 */
	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	bool IsPaused() const { return bPaused; }

	// ---- Widget 访问器 ----
	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowStartWidget* GetStartWidget() const { return StartWidget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowGameWidget* GetGameWidget() const { return GameWidget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowEvaluationWidget* GetEvaluationWidget() const { return EvaluationWidget; }

	/** 从结算界面返回主菜单 */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void ReturnToMainMenu();

	// ---- 蓝图可配置 Widget 类 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowStartWidget> StartWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowGameWidget> GameWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowPauseWidget> PauseWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowEvaluationWidget> EvaluationWidgetClass;

protected:
	UFUNCTION()
	void HandleSimulationEnded();

	/** 退出结算界面时自动调用，回到主菜单 */
	UFUNCTION()
	void HandleEvaluationReturn();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnToMainClicked();

	UFUNCTION()
	void HandleStartGameClicked();

	UFUNCTION()
	void HandleRandomModeClicked();

	UFUNCTION()
	void HandleRestartClicked();

private:
	// ---- Widget 创建 / 切换 ----
	void ShowStartWidget();

	UFUNCTION()
	void ShowGameWidget();

	void ShowGameWidgetRandom();
	void ShowPauseOverlay();
	void HidePauseOverlay();
	void ShowEvaluationWidget();

	// ---- Widget 实例 ----
	UPROPERTY()
	TObjectPtr<UCityFlowStartWidget> StartWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowGameWidget> GameWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowPauseWidget> PauseWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowEvaluationWidget> EvaluationWidget;

	bool bPaused = false;
};
