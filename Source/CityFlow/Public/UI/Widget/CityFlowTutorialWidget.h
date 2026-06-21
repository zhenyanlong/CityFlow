#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "UI/Types/CityFlowTutorialTypes.h"
#include "CityFlowTutorialWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UCityFlowTutorialWidget;

UCLASS()
class CITYFLOW_API UCityFlowTutorialSelectionProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UCityFlowTutorialWidget* InOwner, int32 InEntryIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY()
	TObjectPtr<UCityFlowTutorialWidget> Owner;

	int32 EntryIndex = INDEX_NONE;
};

/** Tutorial browser: entry list on the left, selected text/image on the right. */
UCLASS()
class CITYFLOW_API UCityFlowTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCityFlowTutorialWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackClicked);

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Tutorial")
	FOnBackClicked OnBackClicked;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Tutorial")
	void SelectTutorial(int32 EntryIndex);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Tutorial")
	void RebuildTutorialList();

	/** Reapplies appearance settings to buttons created by the native list builder. */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|Tutorial|Default Entry Appearance")
	void RefreshGeneratedEntryStyles();

	UFUNCTION(BlueprintPure, Category = "CityFlow|Tutorial")
	const UCityFlowTutorialDataAsset* GetTutorialData() const { return TutorialData; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Tutorial")
	TObjectPtr<UCityFlowTutorialDataAsset> TutorialData;

	/** Disable when OnTutorialListRebuilt creates fully custom Blueprint entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Tutorial")
	bool bBuildDefaultEntryButtons = true;

	/** Normal/hovered/pressed appearance of automatically generated entry buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FButtonStyle EntryButtonStyle;

	/** Persistent appearance of the currently selected tutorial entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FButtonStyle SelectedEntryButtonStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FSlateFontInfo EntryTextFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FSlateColor EntryTextColor = FSlateColor(FLinearColor::White);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FSlateColor SelectedEntryTextColor = FSlateColor(FLinearColor(0.1f, 0.8f, 1.0f, 1.0f));

	/** Padding between the button border and its generated text label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FMargin EntryContentPadding = FMargin(12.0f, 8.0f);

	/** Padding around each generated button inside TutorialList. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CityFlow|Tutorial|Default Entry Appearance")
	FMargin EntrySlotPadding = FMargin(0.0f, 0.0f, 0.0f, 4.0f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> TutorialList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TutorialTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TutorialBody;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Tutorial;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

	/** Lets a Blueprint add custom entry buttons instead of using TutorialList. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|Tutorial")
	void OnTutorialListRebuilt(const TArray<FText>& EntryTitles);

	UFUNCTION(BlueprintImplementableEvent, Category = "CityFlow|Tutorial")
	void OnTutorialSelectionChanged(int32 EntryIndex, const FCityFlowTutorialEntry& Entry);

private:
	UFUNCTION()
	void HandleBackClicked();

	UPROPERTY()
	TArray<TObjectPtr<UCityFlowTutorialSelectionProxy>> SelectionProxies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> GeneratedEntryButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GeneratedEntryLabels;

	int32 SelectedTutorialIndex = INDEX_NONE;
};
