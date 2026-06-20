#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "CityFlowDifficultyWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDifficultySelected, ECityFlowDifficulty, Difficulty);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDifficultyBackClicked);

/** Random Mode difficulty picker with a native fallback layout and Blueprint-bindable controls. */
UCLASS()
class CITYFLOW_API UCityFlowDifficultyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Difficulty")
	FOnDifficultySelected OnDifficultySelected;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Difficulty")
	FOnDifficultyBackClicked OnBackClicked;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Difficulty")
	void RefreshProfileDetails();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Easy;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Medium;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Hard;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DifficultyDetails;

private:
	void BuildFallbackLayout();
	static FText FormatProfile(const FCityFlowDifficultyProfile& Profile);
	void ShowProfileDetails(ECityFlowDifficulty Difficulty);

	UFUNCTION()
	void HandleEasyClicked();

	UFUNCTION()
	void HandleMediumClicked();

	UFUNCTION()
	void HandleHardClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleEasyHovered();

	UFUNCTION()
	void HandleMediumHovered();

	UFUNCTION()
	void HandleHardHovered();
};
