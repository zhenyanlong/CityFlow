#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackClicked);

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Tutorial")
	FOnBackClicked OnBackClicked;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Tutorial")
	void SelectTutorial(int32 EntryIndex);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Tutorial")
	void RebuildTutorialList();

	UFUNCTION(BlueprintPure, Category = "CityFlow|Tutorial")
	const UCityFlowTutorialDataAsset* GetTutorialData() const { return TutorialData; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Tutorial")
	TObjectPtr<UCityFlowTutorialDataAsset> TutorialData;

	/** Disable when OnTutorialListRebuilt creates fully custom Blueprint entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Tutorial")
	bool bBuildDefaultEntryButtons = true;

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
};
