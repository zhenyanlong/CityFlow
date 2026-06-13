#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Grid/CityFlowGridTypes.h"
#include "CityFlowPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AGridPlaceableActor;
class AVehicleActor;

UCLASS()
class CITYFLOW_API ACityFlowPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACityFlowPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void EnablePlacement();

	UFUNCTION(BlueprintCallable, Category = "Placement")
	void DisablePlacement();

	UFUNCTION(BlueprintPure, Category = "Placement")
	bool IsPlacementEnabled() const { return bPlacementEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	void SpawnPreview();

	void UpdatePreviewPosition();

	void DestroyPreview();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnPlaceItemStarted();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnRemoveItemStarted();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnPlaceItemTriggered();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnPlaceItemCompleted();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnRemoveItemTriggered();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnRemoveItemCompleted();

	UFUNCTION(BlueprintNativeEvent, Category = "Placement")
	void OnPausePressed();

	class UGridManager* GetGridManager() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_PlaceItem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_RemoveItem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Pause;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSubclassOf<AGridPlaceableActor> PlaceableActorClass;

	UPROPERTY()
	TObjectPtr<AGridPlaceableActor> PreviewActor;

	bool bPlacementEnabled = true;

private:
	void TryPlaceAtCursor();
	void TryRemoveAtCursor();
	void UpdateVehicleHover();
	void ClearHoveredVehicle();
	bool IsSimulationPhaseActive() const;

	FGridVector LastPlacedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
	FGridVector LastRemovedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
	TWeakObjectPtr<AVehicleActor> HoveredVehicle;
};
