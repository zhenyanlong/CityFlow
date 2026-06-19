#include "UI/Widget/CityFlowSettingsWidget.h"

#include "UI/Types/CityFlowMenuSettingsSaveGame.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

#define LOCTEXT_NAMESPACE "CityFlowSettingsWidget"

const FString UCityFlowSettingsWidget::SettingsSlotName = TEXT("CityFlowMenuSettings");

void UCityFlowSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LoadAndApplySettings();

	if (Sld_SoundVolume)
	{
		Sld_SoundVolume->OnValueChanged.RemoveAll(this);
		Sld_SoundVolume->OnValueChanged.AddDynamic(this, &UCityFlowSettingsWidget::HandleSoundVolumeChanged);
	}
	if (Sld_SFXVolume)
	{
		Sld_SFXVolume->OnValueChanged.RemoveAll(this);
		Sld_SFXVolume->OnValueChanged.AddDynamic(this, &UCityFlowSettingsWidget::HandleSFXVolumeChanged);
	}
	if (Cmb_Language)
	{
		Cmb_Language->OnSelectionChanged.RemoveAll(this);
		Cmb_Language->OnSelectionChanged.AddDynamic(this, &UCityFlowSettingsWidget::HandleLanguageChanged);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveAll(this);
		Btn_Back->OnClicked.AddDynamic(this, &UCityFlowSettingsWidget::HandleBackClicked);
	}
}

void UCityFlowSettingsWidget::NativeDestruct()
{
	if (Sld_SoundVolume) Sld_SoundVolume->OnValueChanged.RemoveAll(this);
	if (Sld_SFXVolume) Sld_SFXVolume->OnValueChanged.RemoveAll(this);
	if (Cmb_Language) Cmb_Language->OnSelectionChanged.RemoveAll(this);
	if (Btn_Back) Btn_Back->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowSettingsWidget::LoadAndApplySettings()
{
	Settings = Cast<UCityFlowMenuSettingsSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SettingsSlotName, SettingsUserIndex));
	if (!Settings)
	{
		Settings = Cast<UCityFlowMenuSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UCityFlowMenuSettingsSaveGame::StaticClass()));
	}

	ApplyAudioSettings();
	if (Settings && !Settings->CultureCode.IsEmpty())
	{
		UKismetInternationalizationLibrary::SetCurrentCulture(Settings->CultureCode, true);
	}
	RefreshControls();
}

void UCityFlowSettingsWidget::SetSoundVolume(float Volume)
{
	GetOrCreateSettings()->SoundVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyAudioSettings();
	SaveSettings();
}

void UCityFlowSettingsWidget::SetSFXVolume(float Volume)
{
	GetOrCreateSettings()->SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyAudioSettings();
	SaveSettings();
}

void UCityFlowSettingsWidget::SetLanguage(const FString& CultureCode)
{
	if (CultureCode.IsEmpty())
	{
		return;
	}

	GetOrCreateSettings()->CultureCode = CultureCode;
	SaveSettings();
	UKismetInternationalizationLibrary::SetCurrentCulture(CultureCode, true);
	RefreshControls();
}

float UCityFlowSettingsWidget::GetSoundVolume() const
{
	return Settings ? Settings->SoundVolume : 1.0f;
}

float UCityFlowSettingsWidget::GetSFXVolume() const
{
	return Settings ? Settings->SFXVolume : 1.0f;
}

void UCityFlowSettingsWidget::HandleSoundVolumeChanged(float Value)
{
	SetSoundVolume(Value);
}

void UCityFlowSettingsWidget::HandleSFXVolumeChanged(float Value)
{
	SetSFXVolume(Value);
}

void UCityFlowSettingsWidget::HandleLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	SetLanguage(SelectedItem == LOCTEXT("ChineseLanguageName", "中文").ToString()
		? ChineseCultureCode
		: EnglishCultureCode);
}

void UCityFlowSettingsWidget::HandleBackClicked()
{
	OnBackClicked.Broadcast();
}

void UCityFlowSettingsWidget::ApplyAudioSettings() const
{
	if (!Settings || !SoundMix || !GetWorld())
	{
		return;
	}

	UGameplayStatics::PushSoundMixModifier(GetWorld(), SoundMix);
	if (SoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(
			GetWorld(), SoundMix, SoundClass, Settings->SoundVolume, 1.0f, 0.0f, true);
	}
	if (SFXSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(
			GetWorld(), SoundMix, SFXSoundClass, Settings->SFXVolume, 1.0f, 0.0f, true);
	}
}

void UCityFlowSettingsWidget::SaveSettings() const
{
	if (Settings)
	{
		UGameplayStatics::SaveGameToSlot(Settings, SettingsSlotName, SettingsUserIndex);
	}
}

void UCityFlowSettingsWidget::RefreshControls()
{
	if (!Settings)
	{
		return;
	}

	if (Sld_SoundVolume) Sld_SoundVolume->SetValue(Settings->SoundVolume);
	if (Sld_SFXVolume) Sld_SFXVolume->SetValue(Settings->SFXVolume);

	if (Cmb_Language)
	{
		const FString EnglishOption = LOCTEXT("EnglishLanguageName", "English").ToString();
		const FString ChineseOption = LOCTEXT("ChineseLanguageName", "中文").ToString();
		Cmb_Language->ClearOptions();
		Cmb_Language->AddOption(EnglishOption);
		Cmb_Language->AddOption(ChineseOption);

		const FString ActiveCulture = Settings->CultureCode.IsEmpty()
			? FInternationalization::Get().GetCurrentCulture()->GetName()
			: Settings->CultureCode;
		Cmb_Language->SetSelectedOption(ActiveCulture.StartsWith(TEXT("zh")) ? ChineseOption : EnglishOption);
	}
}

UCityFlowMenuSettingsSaveGame* UCityFlowSettingsWidget::GetOrCreateSettings()
{
	if (!Settings)
	{
		Settings = Cast<UCityFlowMenuSettingsSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UCityFlowMenuSettingsSaveGame::StaticClass()));
	}
	return Settings;
}

#undef LOCTEXT_NAMESPACE
