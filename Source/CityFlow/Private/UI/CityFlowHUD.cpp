#include "UI/CityFlowHUD.h"
#include "Blueprint/UserWidget.h"

void ACityFlowHUD::BeginPlay()
{
	Super::BeginPlay();
	ShowGameWidget();
}

void ACityFlowHUD::ShowGameWidget()
{
	if (!GameWidget && GameWidgetClass)
	{
		GameWidget = CreateWidget<UUserWidget>(GetWorld(), GameWidgetClass);
	}

	if (GameWidget && !GameWidget->IsInViewport())
	{
		GameWidget->AddToViewport();
	}

	if (EvaluationWidget && EvaluationWidget->IsInViewport())
	{
		EvaluationWidget->RemoveFromParent();
	}
}

void ACityFlowHUD::HideGameWidget()
{
	if (GameWidget && GameWidget->IsInViewport())
	{
		GameWidget->RemoveFromParent();
	}
}

void ACityFlowHUD::ShowEvaluationWidget()
{
	if (!EvaluationWidget && EvaluationWidgetClass)
	{
		EvaluationWidget = CreateWidget<UUserWidget>(GetWorld(), EvaluationWidgetClass);
	}

	if (EvaluationWidget && !EvaluationWidget->IsInViewport())
	{
		EvaluationWidget->AddToViewport();
	}

	if (GameWidget && GameWidget->IsInViewport())
	{
		GameWidget->RemoveFromParent();
	}
}
