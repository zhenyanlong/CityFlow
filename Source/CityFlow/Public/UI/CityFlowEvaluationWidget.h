#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "CityFlowEvaluationWidget.generated.h"

/** 结算界面 —— 展示模拟结果与历史最高分，提供返回主菜单 / 重新开始 */
UCLASS()
class CITYFLOW_API UCityFlowEvaluationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackToMainClicked);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRestartClicked);

	/** 点击 "返回主菜单" */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnBackToMainClicked OnBackToMainClicked;

	/** 点击 "重新开始" */
	UPROPERTY(BlueprintAssignable, Category = "CityFlow|UI")
	FOnRestartClicked OnRestartClicked;

	/** 设置本局所有结算数据并刷新 UI */
	UFUNCTION(BlueprintCallable, Category = "CityFlow|UI")
	void Populate(int32 TotalScore, int32 Arrivals, int32 Penalty, float ElapsedTime);

protected:
	// ---- BindWidget 控件 ----

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_BackToMain;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Restart;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_TotalScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Arrivals;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Penalty;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_HighScore;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SimulationTime;

private:
	UFUNCTION()
	void HandleBackToMainClicked();

	UFUNCTION()
	void HandleRestartClicked();

	void RefreshUI();

	int32 CachedTotalScore = 0;
	int32 CachedArrivals = 0;
	int32 CachedPenalty = 0;
	float CachedElapsedTime = 0.0f;

	/** 运行时历史最高分（进程生命期内有效） */
	static int32 GlobalHighScore;
};
