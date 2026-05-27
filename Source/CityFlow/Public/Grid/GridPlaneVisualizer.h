#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridPlaneVisualizer.generated.h"

UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API AGridPlaneVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AGridPlaneVisualizer();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void SetupPlane();

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void UpdateMaterialParams();

	UFUNCTION(BlueprintCallable, Category = "Grid|Visualizer")
	void SetGridVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Grid|Visualizer")
	bool IsGridVisible() const { return bGridVisible; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Material")
	TObjectPtr<class UMaterialInterface> GridMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance")
	FLinearColor LineColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance", meta = (ClampMin = "0.002", ClampMax = "0.2"))
	float LineWidth = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer|Appearance")
	float ZOffset = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Visualizer")
	bool bAutoSetup = true;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|Visualizer")
	bool bPlaneSetup = false;

private:
	class UGridManager* GetGridManager() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> PlaneComponent;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> DynamicMaterial;

	bool bGridVisible = false;

	static constexpr float kDefaultPlaneSize = 1000.0f;
};
