#pragma once

#include "CoreMinimal.h"
#include "Grid/GridPlaceableActor.h"
#include "MeshGridPlaceableActor.generated.h"

UCLASS(Abstract)
class CITYFLOW_API AMeshGridPlaceableActor : public AGridPlaceableActor
{
	GENERATED_BODY()

public:
	AMeshGridPlaceableActor();

protected:
	virtual void OnEnterPreview_Implementation() override;
	virtual void OnEnterPlaced_Implementation() override;
	virtual void OnPreviewValidChanged_Implementation(bool bValid) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<class UMaterialInterface> PreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<class UMaterialInterface> InvalidPreviewMaterial;

private:
	void ApplyMaterialToAllSlots(UMaterialInterface* Material);
	void RestoreOriginalMaterials();

	UPROPERTY()
	TArray<TObjectPtr<class UMaterialInterface>> OriginalMaterials;
};
