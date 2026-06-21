#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CityFlowPauseWidget.generated.h"

/** Modal pause overlay displayed above the active gameplay HUD. */
UCLASS()
class CITYFLOW_API UCityFlowPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResumeClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReturnToMainClicked);

	/** Broadcast when the player selects Resume. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnResumeClicked OnResumeClicked;

	/** Broadcast when the player selects Back to Main Menu. */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnReturnToMainClicked OnReturnToMainClicked;

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_ReturnToMain;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnToMainClicked();
};
