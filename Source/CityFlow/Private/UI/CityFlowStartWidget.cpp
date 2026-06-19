#include "UI/CityFlowStartWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UCityFlowStartWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Backward compatibility for the existing WBP_StartWidget asset. The old
	// button can be deleted from the Blueprint later; it is hidden immediately.
	if (UWidget* LegacyStartButton = WidgetTree->FindWidget(TEXT("Btn_StartGame")))
		LegacyStartButton->SetVisibility(ESlateVisibility::Collapsed);

	if (Btn_RandomMode)
		Btn_RandomMode->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleRandomModeClicked);
	if (Btn_Tutorial)
		Btn_Tutorial->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleTutorialClicked);
	if (Btn_Settings)
		Btn_Settings->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleSettingsClicked);
	if (Btn_QuitGame)
		Btn_QuitGame->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleQuitGameClicked);
}

void UCityFlowStartWidget::NativeDestruct()
{
	if (Btn_RandomMode)
		Btn_RandomMode->OnClicked.RemoveAll(this);
	if (Btn_Tutorial)
		Btn_Tutorial->OnClicked.RemoveAll(this);
	if (Btn_Settings)
		Btn_Settings->OnClicked.RemoveAll(this);
	if (Btn_QuitGame)
		Btn_QuitGame->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowStartWidget::HandleRandomModeClicked()
{
	OnRandomModeClicked.Broadcast();
}

void UCityFlowStartWidget::HandleTutorialClicked()
{
	OnTutorialClicked.Broadcast();
}

void UCityFlowStartWidget::HandleSettingsClicked()
{
	OnSettingsClicked.Broadcast();
}

void UCityFlowStartWidget::HandleQuitGameClicked()
{
	OnQuitGameClicked.Broadcast();
}
