#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "CityFlowStartWidget.generated.h"

/** Main-menu widget that exposes game-mode, tutorial, settings, and quit actions. */
UCLASS()
class CITYFLOW_API UCityFlowStartWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRandomModeClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTutorialClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingsClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuitGameClicked);

	/** Broadcast when the player selects Random Mode. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnRandomModeClicked OnRandomModeClicked;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnTutorialClicked OnTutorialClicked;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnSettingsClicked OnSettingsClicked;

	/** Broadcast when the player selects Quit Game. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnQuitGameClicked OnQuitGameClicked;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_RandomMode;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Tutorial;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_QuitGame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Version;

private:
	UFUNCTION()
	void HandleRandomModeClicked();

	UFUNCTION()
	void HandleTutorialClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitGameClicked();
};
