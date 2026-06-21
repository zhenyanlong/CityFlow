#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowHUD.generated.h"

class UCityFlowStartWidget;
class UCityFlowGameWidget;
class UCityFlowPauseWidget;
class UCityFlowEvaluationWidget;
class UCityFlowTutorialWidget;
class UCityFlowSettingsWidget;
class UCityFlowDifficultyWidget;
class UAudioComponent;
class USoundBase;
class USoundClass;

/**
 * Owns the complete widget lifecycle:
 *   StartWidget → Tutorial/Settings or Random Game → Pause/Evaluation → (loop)
 *
 *   BeginPlay shows StartWidget; Random Mode enters Planning;
 *   Escape toggles pause, and Evaluation can return to the menu.
 */
UCLASS()
class CITYFLOW_API ACityFlowHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/** Toggles pause; called by the player controller's Escape input. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void TogglePause();

	/** Returns whether this HUD currently owns an active pause modal. */
	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	bool IsPaused() const { return bPaused; }

	// ---- Widget accessors ----
	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowStartWidget* GetStartWidget() const { return StartWidget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowGameWidget* GetGameWidget() const { return GameWidget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	UCityFlowEvaluationWidget* GetEvaluationWidget() const { return EvaluationWidget; }

	/** Leaves Evaluation and requests authoritative main-menu teardown. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void ReturnToMainMenu();

	// ---- Blueprint-configurable widget classes ----
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

	/** Optional custom Blueprint. When unset, the native difficulty widget builds a usable fallback UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<UCityFlowDifficultyWidget> DifficultyWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	bool bEnableMainMenuPreviewMatch = true;

	/** Background music started with the HUD. Use a looping SoundCue/MetaSound for continuous playback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio")
	TObjectPtr<USoundBase> BackgroundMusic;

	/** Assign SC_Music here; it should be a child of the master SoundClass controlled by Settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio")
	TObjectPtr<USoundClass> BackgroundMusicSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Audio", meta = (ClampMin = "0.0"))
	float BackgroundMusicVolumeMultiplier = 1.0f;

protected:
	UFUNCTION()
	void HandleSimulationEnded();

	/** Handles the evaluation widget's Back to Main Menu action. */
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
	void HandleDifficultySelected(ECityFlowDifficulty Difficulty);

	UFUNCTION()
	void HandleMenuPanelBackClicked();

	UFUNCTION()
	void HandleRestartClicked();

private:
	// ---- Widget creation and transitions ----
	void ShowStartWidget();

	void ShowGameWidgetRandom(ECityFlowDifficulty Difficulty);
	void ShowDifficultyWidget();
	void ShowTutorialWidget();
	void ShowSettingsWidget();
	void ShowPauseOverlay();
	void HidePauseOverlay();
	void ShowEvaluationWidget();
	void StartBackgroundMusic();

	// ---- Live widget instances ----
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

	UPROPERTY()
	TObjectPtr<UCityFlowDifficultyWidget> DifficultyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	bool bPaused = false;
};
