#include "UI/CityFlowPauseWidget.h"

void UCityFlowPauseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Resume)
		Btn_Resume->OnClicked.AddDynamic(this, &UCityFlowPauseWidget::HandleResumeClicked);
	if (Btn_ReturnToMain)
		Btn_ReturnToMain->OnClicked.AddDynamic(this, &UCityFlowPauseWidget::HandleReturnToMainClicked);
}

void UCityFlowPauseWidget::NativeDestruct()
{
	if (Btn_Resume)
		Btn_Resume->OnClicked.RemoveAll(this);
	if (Btn_ReturnToMain)
		Btn_ReturnToMain->OnClicked.RemoveAll(this);

	Super::NativeDestruct();
}

void UCityFlowPauseWidget::HandleResumeClicked()
{
	OnResumeClicked.Broadcast();
}

void UCityFlowPauseWidget::HandleReturnToMainClicked()
{
	OnReturnToMainClicked.Broadcast();
}
