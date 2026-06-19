#include "UI/Widget/CityFlowTutorialWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

void UCityFlowTutorialSelectionProxy::Initialize(UCityFlowTutorialWidget* InOwner, int32 InEntryIndex)
{
	Owner = InOwner;
	EntryIndex = InEntryIndex;
}

void UCityFlowTutorialSelectionProxy::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectTutorial(EntryIndex);
	}
}

void UCityFlowTutorialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveAll(this);
		Btn_Back->OnClicked.AddDynamic(this, &UCityFlowTutorialWidget::HandleBackClicked);
	}

	RebuildTutorialList();
}

void UCityFlowTutorialWidget::NativeDestruct()
{
	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveAll(this);
	}

	SelectionProxies.Reset();
	Super::NativeDestruct();
}

void UCityFlowTutorialWidget::RebuildTutorialList()
{
	SelectionProxies.Reset();

	TArray<FText> EntryTitles;
	if (TutorialData)
	{
		EntryTitles.Reserve(TutorialData->Entries.Num());
		for (const FCityFlowTutorialEntry& Entry : TutorialData->Entries)
		{
			EntryTitles.Add(Entry.Title);
		}
	}

	OnTutorialListRebuilt(EntryTitles);

	if (TutorialList && bBuildDefaultEntryButtons)
	{
		TutorialList->ClearChildren();

		for (int32 Index = 0; TutorialData && Index < TutorialData->Entries.Num(); ++Index)
		{
			UButton* EntryButton = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), *FString::Printf(TEXT("TutorialEntry_%d"), Index));
			UTextBlock* EntryLabel = WidgetTree->ConstructWidget<UTextBlock>();
			EntryLabel->SetText(TutorialData->Entries[Index].Title);
			EntryLabel->SetJustification(ETextJustify::Left);
			EntryButton->AddChild(EntryLabel);

			UCityFlowTutorialSelectionProxy* Proxy = NewObject<UCityFlowTutorialSelectionProxy>(this);
			Proxy->Initialize(this, Index);
			EntryButton->OnClicked.AddDynamic(Proxy, &UCityFlowTutorialSelectionProxy::HandleClicked);
			SelectionProxies.Add(Proxy);
			TutorialList->AddChildToVerticalBox(EntryButton);
		}
	}

	if (TutorialData && !TutorialData->Entries.IsEmpty())
	{
		SelectTutorial(0);
	}
	else
	{
		if (Txt_TutorialTitle) Txt_TutorialTitle->SetText(FText::GetEmpty());
		if (Txt_TutorialBody) Txt_TutorialBody->SetText(FText::GetEmpty());
		if (Img_Tutorial) Img_Tutorial->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCityFlowTutorialWidget::SelectTutorial(int32 EntryIndex)
{
	if (!TutorialData || !TutorialData->Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	const FCityFlowTutorialEntry& Entry = TutorialData->Entries[EntryIndex];
	if (Txt_TutorialTitle) Txt_TutorialTitle->SetText(Entry.Title);
	if (Txt_TutorialBody) Txt_TutorialBody->SetText(Entry.Body);

	if (Img_Tutorial)
	{
		if (UTexture2D* Texture = Entry.Image.LoadSynchronous())
		{
			Img_Tutorial->SetBrushFromTexture(Texture, true);
			Img_Tutorial->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Img_Tutorial->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	OnTutorialSelectionChanged(EntryIndex, Entry);
}

void UCityFlowTutorialWidget::HandleBackClicked()
{
	OnBackClicked.Broadcast();
}
