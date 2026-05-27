#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CityFlowPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AGridPlaceableActor;

UCLASS()
class CITYFLOW_API ACityFlowPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACityFlowPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	void SpawnPreview();

	void UpdatePreviewPosition();

	void DestroyPreview();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnPlaceItemStarted();

	class UGridManager* GetGridManager() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_PlaceItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSubclassOf<AGridPlaceableActor> PlaceableActorClass;

	UPROPERTY()
	TObjectPtr<AGridPlaceableActor> PreviewActor;
};
