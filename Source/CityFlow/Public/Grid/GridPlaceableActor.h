#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/CityFlowGridTypes.h"
#include "GridPlaceableActor.generated.h"

UCLASS(Abstract)
class CITYFLOW_API AGridPlaceableActor : public AActor
{
	GENERATED_BODY()

public:
	AGridPlaceableActor();

	UFUNCTION(BlueprintCallable, Category = "Grid|State")
	void EnterPreviewState();

	UFUNCTION(BlueprintPure, Category = "Grid|State")
	bool IsPreview() const { return bIsPreview; }

	UFUNCTION(BlueprintPure, Category = "Grid|State")
	bool IsPlacedOnGrid() const { return bIsPlaced; }

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool PlaceOnGrid(const FGridVector& InGridPos);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool PlaceOnGridAtWorld(const FVector& WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	void RemoveFromGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool CanPlaceAt(const FGridVector& InGridPos) const;

	UFUNCTION(BlueprintCallable, Category = "Grid|Placement")
	bool SnapToGridPosition(const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	FGridVector GetGridPosition() const { return GridPosition; }

	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	FVector GetGridWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Grid|Placement")
	const TArray<FGridVector>& GetOccupiedCells() const { return OccupiedCells; }

	UFUNCTION(BlueprintCallable, Category = "Grid|State")
	void SetPreviewPlacementValid(bool bValid);

	UFUNCTION(BlueprintPure, Category = "Grid|State")
	bool IsPreviewPlacementValid() const { return bPreviewPlacementValid; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Grid|State")
	void OnEnterPreview();

	UFUNCTION(BlueprintNativeEvent, Category = "Grid|State")
	void OnEnterPlaced();

	UFUNCTION(BlueprintNativeEvent, Category = "Grid|Placement")
	void OnPlacedOnGrid();

	UFUNCTION(BlueprintNativeEvent, Category = "Grid|Placement")
	void OnRemovedFromGrid();

	UFUNCTION(BlueprintNativeEvent, Category = "Grid|State")
	void OnPreviewValidChanged(bool bValid);

	virtual ECellType GetPlacementCellType() const { return ECellType::Building; }

	virtual TArray<FGridVector> CalculateOccupiedCells(const FGridVector& BasePos) const;

	virtual bool ValidatePlacement(const FGridVector& BasePos) const;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|State")
	bool bIsPreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|State")
	bool bIsPlaced = false;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|State")
	bool bPreviewPlacementValid = true;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|Placement")
	FGridVector GridPosition;

	UPROPERTY(BlueprintReadOnly, Category = "Grid|Placement")
	TArray<FGridVector> OccupiedCells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Placement")
	FVector2D BuildingSize = FVector2D(1, 1);

	class UGridManager* GetGridManager() const;

private:
	void EnterPlacedState();

	void RegisterCells();
	void UnregisterCells();
};
