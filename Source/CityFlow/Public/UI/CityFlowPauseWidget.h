#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "CityFlowPauseWidget.generated.h"

/** 暂停菜单 —— 以 Overlay 形式叠加在当前画面上 */
UCLASS()
class CITYFLOW_API UCityFlowPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResumeClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReturnToMainClicked);

	/** 点击 "继续" 时广播 */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnResumeClicked OnResumeClicked;

	/** 点击 "返回主菜单" 时广播 */
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
