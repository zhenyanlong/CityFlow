#include "UI/Widget/CityFlowDifficultyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameMode/CityFlowGameMode.h"

#define LOCTEXT_NAMESPACE "CityFlowDifficultyWidget"

void UCityFlowDifficultyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Btn_Easy && !Btn_Medium && !Btn_Hard)
	{
		BuildFallbackLayout();
	}

	if (Btn_Easy)
	{
		Btn_Easy->OnClicked.RemoveAll(this);
		Btn_Easy->OnClicked.AddDynamic(this, &UCityFlowDifficultyWidget::HandleEasyClicked);
		Btn_Easy->OnHovered.RemoveAll(this);
		Btn_Easy->OnHovered.AddDynamic(this, &UCityFlowDifficultyWidget::HandleEasyHovered);
	}
	if (Btn_Medium)
	{
		Btn_Medium->OnClicked.RemoveAll(this);
		Btn_Medium->OnClicked.AddDynamic(this, &UCityFlowDifficultyWidget::HandleMediumClicked);
		Btn_Medium->OnHovered.RemoveAll(this);
		Btn_Medium->OnHovered.AddDynamic(this, &UCityFlowDifficultyWidget::HandleMediumHovered);
	}
	if (Btn_Hard)
	{
		Btn_Hard->OnClicked.RemoveAll(this);
		Btn_Hard->OnClicked.AddDynamic(this, &UCityFlowDifficultyWidget::HandleHardClicked);
		Btn_Hard->OnHovered.RemoveAll(this);
		Btn_Hard->OnHovered.AddDynamic(this, &UCityFlowDifficultyWidget::HandleHardHovered);
	}
	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveAll(this);
		Btn_Back->OnClicked.AddDynamic(this, &UCityFlowDifficultyWidget::HandleBackClicked);
	}

	RefreshProfileDetails();
}

void UCityFlowDifficultyWidget::NativeDestruct()
{
	if (Btn_Easy) { Btn_Easy->OnClicked.RemoveAll(this); Btn_Easy->OnHovered.RemoveAll(this); }
	if (Btn_Medium) { Btn_Medium->OnClicked.RemoveAll(this); Btn_Medium->OnHovered.RemoveAll(this); }
	if (Btn_Hard) { Btn_Hard->OnClicked.RemoveAll(this); Btn_Hard->OnHovered.RemoveAll(this); }
	if (Btn_Back) Btn_Back->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowDifficultyWidget::BuildFallbackLayout()
{
	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DifficultyRoot"));
	RootBorder->SetPadding(FMargin(32.0f));
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DifficultyPanel"));
	RootBorder->SetContent(Panel);

	auto AddText = [this, Panel](const FText& Text, float BottomPadding)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		Label->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* Slot = Panel->AddChildToVerticalBox(Label);
		Slot->SetPadding(FMargin(4.0f, 4.0f, 4.0f, BottomPadding));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		return Label;
	};

	AddText(LOCTEXT("DifficultyTitle", "Select Difficulty"), 8.0f);
	AddText(LOCTEXT("DifficultySubtitle", "Choose the scale and traffic pressure for this city."), 20.0f);

	auto AddDifficulty = [this, Panel](const FText& LabelText, TObjectPtr<UButton>& OutButton)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
		ButtonLabel->SetText(LabelText);
		ButtonLabel->SetJustification(ETextJustify::Center);
		OutButton->AddChild(ButtonLabel);
		UVerticalBoxSlot* ButtonSlot = Panel->AddChildToVerticalBox(OutButton);
		ButtonSlot->SetPadding(FMargin(4.0f));
	};

	AddDifficulty(LOCTEXT("EasyLabel", "Easy"), Btn_Easy);
	AddDifficulty(LOCTEXT("MediumLabel", "Medium"), Btn_Medium);
	AddDifficulty(LOCTEXT("HardLabel", "Hard"), Btn_Hard);
	Txt_DifficultyDetails = AddText(FText::GetEmpty(), 12.0f);

	Btn_Back = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>();
	BackLabel->SetText(LOCTEXT("BackLabel", "Back"));
	BackLabel->SetJustification(ETextJustify::Center);
	Btn_Back->AddChild(BackLabel);
	UVerticalBoxSlot* BackSlot = Panel->AddChildToVerticalBox(Btn_Back);
	BackSlot->SetPadding(FMargin(4.0f, 12.0f, 4.0f, 4.0f));
}

FText UCityFlowDifficultyWidget::FormatProfile(const FCityFlowDifficultyProfile& Profile)
{
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1);
	OneDecimal.SetMaximumFractionalDigits(2);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Buildings"), FText::AsNumber(Profile.BuildingCount));
	Args.Add(TEXT("Interval"), FText::AsNumber(Profile.VehicleSpawnInterval, &OneDecimal));
	Args.Add(TEXT("Duration"), FText::AsNumber(FMath::RoundToInt(Profile.SimulationDuration)));
	Args.Add(TEXT("Budget"), FText::AsNumber(Profile.RoadBudget));
	return FText::Format(
		LOCTEXT("ProfileFormat", "{Buildings} buildings | {Interval}s spawn | {Duration}s runtime | {Budget} budget"),
		Args);
}

void UCityFlowDifficultyWidget::RefreshProfileDetails()
{
	ShowProfileDetails(ECityFlowDifficulty::Medium);
}

void UCityFlowDifficultyWidget::ShowProfileDetails(ECityFlowDifficulty Difficulty)
{
	const ACityFlowGameMode* GameMode = GetWorld()
		? Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode())
		: nullptr;
	if (GameMode && Txt_DifficultyDetails)
	{
		Txt_DifficultyDetails->SetText(FormatProfile(GameMode->GetDifficultyProfile(Difficulty)));
	}
}

void UCityFlowDifficultyWidget::HandleEasyClicked()
{
	OnDifficultySelected.Broadcast(ECityFlowDifficulty::Easy);
}

void UCityFlowDifficultyWidget::HandleMediumClicked()
{
	OnDifficultySelected.Broadcast(ECityFlowDifficulty::Medium);
}

void UCityFlowDifficultyWidget::HandleHardClicked()
{
	OnDifficultySelected.Broadcast(ECityFlowDifficulty::Hard);
}

void UCityFlowDifficultyWidget::HandleBackClicked()
{
	OnBackClicked.Broadcast();
}

void UCityFlowDifficultyWidget::HandleEasyHovered()
{
	ShowProfileDetails(ECityFlowDifficulty::Easy);
}

void UCityFlowDifficultyWidget::HandleMediumHovered()
{
	ShowProfileDetails(ECityFlowDifficulty::Medium);
}

void UCityFlowDifficultyWidget::HandleHardHovered()
{
	ShowProfileDetails(ECityFlowDifficulty::Hard);
}

#undef LOCTEXT_NAMESPACE
