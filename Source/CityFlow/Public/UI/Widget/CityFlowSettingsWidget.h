#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "CityFlowSettingsWidget.generated.h"

class UButton;
class USlider;
class USoundClass;
class USoundMix;
class UCityFlowMenuSettingsSaveGame;

UCLASS()
class CITYFLOW_API UCityFlowSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackClicked);

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Settings")
	FOnBackClicked OnBackClicked;

	/** Loads persisted settings and applies audio/culture. Safe to call before adding the widget to the viewport. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Settings")
	void LoadAndApplySettings();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Settings")
	void SetSoundVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Settings")
	void SetSFXVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Settings")
	void SetLanguage(const FString& CultureCode);

	UFUNCTION(BlueprintPure, Category = "CityFlow|Settings")
	float GetSoundVolume() const;

	UFUNCTION(BlueprintPure, Category = "CityFlow|Settings")
	float GetSFXVolume() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Settings|Audio")
	TObjectPtr<USoundMix> SoundMix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Settings|Audio")
	TObjectPtr<USoundClass> SoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Settings|Audio")
	TObjectPtr<USoundClass> SFXSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Settings|Language")
	FString EnglishCultureCode = TEXT("en");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Settings|Language")
	FString ChineseCultureCode = TEXT("zh-Hans");

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Sld_SoundVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> Sld_SFXVolume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> Cmb_Language;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

private:
	UFUNCTION()
	void HandleSoundVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleBackClicked();

	void ApplyAudioSettings() const;
	void SaveSettings() const;
	void RefreshControls();
	UCityFlowMenuSettingsSaveGame* GetOrCreateSettings();

	UPROPERTY(Transient)
	TObjectPtr<UCityFlowMenuSettingsSaveGame> Settings;

	static const FString SettingsSlotName;
	static constexpr int32 SettingsUserIndex = 0;
};
