#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CityFlowHUD.generated.h"

UCLASS()
class CITYFLOW_API ACityFlowHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void ShowGameWidget();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void HideGameWidget();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void ShowEvaluationWidget();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<class UUserWidget> GameWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|UI")
	TSubclassOf<class UUserWidget> EvaluationWidgetClass;

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	class UUserWidget* GetGameWidget() const { return GameWidget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|UI")
	class UUserWidget* GetEvaluationWidget() const { return EvaluationWidget; }

private:
	UPROPERTY()
	TObjectPtr<class UUserWidget> GameWidget;

	UPROPERTY()
	TObjectPtr<class UUserWidget> EvaluationWidget;
};
