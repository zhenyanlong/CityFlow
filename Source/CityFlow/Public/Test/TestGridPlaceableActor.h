#pragma once

#include "CoreMinimal.h"
#include "Grid/GridPlaceableActor.h"
#include "TestGridPlaceableActor.generated.h"

UCLASS()
class CITYFLOW_API ATestGridPlaceableActor : public AGridPlaceableActor
{
	GENERATED_BODY()

public:
	ATestGridPlaceableActor();

protected:
	virtual void BeginPreview_Implementation() override;
	virtual void OnPlacedOnGrid_Implementation() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor PlacedColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor PreviewColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	float PreviewOpacity = 0.3f;

	UPROPERTY()
	TObjectPtr<class UMaterialInstanceDynamic> PreviewMaterial;
};
