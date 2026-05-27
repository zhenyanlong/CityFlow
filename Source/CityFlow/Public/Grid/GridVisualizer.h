#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridVisualizer.generated.h"

UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API AGridVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AGridVisualizer();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void DrawGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void ClearGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void RedrawGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void SetGridVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Grid|Visualizer")
	bool IsGridVisible() const { return bGridVisible; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance")
	FLinearColor GridLineColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance", meta = (ClampMin = "0.1", ClampMax = "500.0"))
	float GridLineThickness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance", meta = (ClampMin = "0.0"))
	float GridLineZOffset = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer")
	bool bShowOnBeginPlay = true;

private:
	void DrawGridInternal();

	class UGridManager* GetGridManager() const;

	UPROPERTY()
	TObjectPtr<class ULineBatchComponent> LineBatchComponent;

	bool bGridVisible = false;
	bool bGridDrawn = false;
};
