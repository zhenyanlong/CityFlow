#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CityFlowHUD.generated.h"

class UCityFlowStartWidget;
class UCityFlowGameWidget;
class UCityFlowPauseWidget;
class UCityFlowEvaluationWidget;
class UCityFlowTutorialWidget;
class UCityFlowSettingsWidget;
class UAudioComponent;
class USoundBase;
class USoundClass;

/**
 * HUD —— 管理完整 Widget 生命周期：
 *   StartWidget → Tutorial/Settings or Random Game → Pause/Evaluation → (loop)
 *
 *   BeginPlay 显示 StartWidget；点击 Random Mode 进入 Planning；
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowTutorialWidget> TutorialWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowSettingsWidget> SettingsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	bool bEnableMainMenuPreviewMatch = true;

	/** Background music started with the HUD. Use a looping SoundCue/MetaSound for continuous playback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio")
	TObjectPtr<USoundBase> BackgroundMusic;

	/** Assign the master SoundClass here so the Sound slider always controls the music. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio")
	TObjectPtr<USoundClass> BackgroundMusicSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio", meta = (ClampMin = "0.0"))
	float BackgroundMusicVolumeMultiplier = 1.0f;

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
	void HandleRandomModeClicked();

	UFUNCTION()
	void HandleTutorialClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitGameClicked();

	UFUNCTION()
	void HandleMenuPanelBackClicked();

	UFUNCTION()
	void HandleRestartClicked();

private:
	// ---- Widget 创建 / 切换 ----
	void ShowStartWidget();

	void ShowGameWidgetRandom();
	void ShowTutorialWidget();
	void ShowSettingsWidget();
	void ShowPauseOverlay();
	void HidePauseOverlay();
	void ShowEvaluationWidget();
	void StartBackgroundMusic();

	// ---- Widget 实例 ----
	UPROPERTY()
	TObjectPtr<UCityFlowStartWidget> StartWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowGameWidget> GameWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowPauseWidget> PauseWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowEvaluationWidget> EvaluationWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowTutorialWidget> TutorialWidget;

	UPROPERTY()
	TObjectPtr<UCityFlowSettingsWidget> SettingsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	bool bPaused = false;
};
