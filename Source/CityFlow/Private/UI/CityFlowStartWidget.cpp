#include "UI/CityFlowStartWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UCityFlowStartWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_StartGame)
		Btn_StartGame->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleStartGameClicked);
	if (Btn_QuitGame)
		Btn_QuitGame->OnClicked.AddDynamic(this, &UCityFlowStartWidget::HandleQuitGameClicked);
}

void UCityFlowStartWidget::NativeDestruct()
{
	if (Btn_StartGame)
		Btn_StartGame->OnClicked.RemoveAll(this);
	if (Btn_QuitGame)
		Btn_QuitGame->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowStartWidget::HandleStartGameClicked()
{
	OnStartGameClicked.Broadcast();
}

void UCityFlowStartWidget::HandleQuitGameClicked()
{
	OnQuitGameClicked.Broadcast();
}
