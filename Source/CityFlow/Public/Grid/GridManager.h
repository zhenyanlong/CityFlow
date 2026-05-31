#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Grid/CityFlowGridTypes.h"
#include "GridManager.generated.h"

class AGridPlaceableActor;

USTRUCT(BlueprintType)
struct FBuildingSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AGridPlaceableActor> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 Count = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCellChanged, FGridVector, CellPos, const FGridCell&, NewCell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGridPlaced, AGridPlaceableActor*, PlacedActor);

UCLASS()
class CITYFLOW_API UGridManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Grid")
	void InitGrid(int32 InGridWidth, int32 InGridHeight, float InCellSize, const FVector& InGridOrigin);

	UFUNCTION(BlueprintPure, Category = "Grid")
	FGridVector WorldToGrid(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GridToWorld(const FGridVector& GridPos) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector SnapToGrid(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsValidGridPos(const FGridVector& GridPos) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	const FGridCell& GetCell(const FGridVector& GridPos) const;

	UFUNCTION(BlueprintCallable, Category = "Grid")
	bool OccupyCell(const FGridVector& GridPos, ECellType Type, int32 BuildingID, AActor* RoadActor);

	bool OccupyCell(const FGridVector& GridPos, ECellType Type);

	UFUNCTION(BlueprintCallable, Category = "Grid")
	bool ClearCell(const FGridVector& GridPos);

	UFUNCTION(BlueprintPure, Category = "Grid")
	ECellType GetCellType(const FGridVector& GridPos) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	TArray<FGridVector> GetNeighbors(const FGridVector& GridPos, bool bCardinalOnly = true) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	TArray<FGridVector> GetCellsOfType(ECellType Type) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool HasAdjacentType(const FGridVector& GridPos, ECellType Type) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	uint8 CalculateConnectedMask(const FGridVector& GridPos) const;

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsGridInitialized() const { return bGridInitialized; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	int32 GetGridWidth() const { return GridWidth; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	int32 GetGridHeight() const { return GridHeight; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	float GetCellSize() const { return CellSize; }

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetGridOrigin() const { return GridOrigin; }

	UFUNCTION(BlueprintCallable, Category = "Grid|Budget")
	void SetRoadBudget(int32 InBudget);

	UFUNCTION(BlueprintPure, Category = "Grid|Budget")
	int32 GetRemainingBudget() const { return RoadBudget; }

	UFUNCTION(BlueprintCallable, Category = "Grid|Budget")
	bool ConsumeRoadBudget(int32 Count = 1);

	UFUNCTION(BlueprintCallable, Category = "Grid|Budget")
	void AddRoadBudget(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Grid|Debug")
	AGridPlaceableActor* TryPlaceBuildingRandom(TSubclassOf<AGridPlaceableActor> PlaceableClass);

	UFUNCTION(BlueprintCallable, Category = "Grid|Debug")
	TArray<AGridPlaceableActor*> TryPlaceBuildingsRandom(const TArray<FBuildingSpawnRequest>& Requests);

	UPROPERTY(BlueprintAssignable, Category = "Grid|Events")
	FOnCellChanged OnCellChanged;

	UPROPERTY(BlueprintAssignable, Category = "Grid|Events")
	FOnGridPlaced OnGridPlaced;

	void UpdateNeighborMasks(const FGridVector& GridPos);

protected:

private:
	TArray<TArray<FGridCell>> Grid;
	int32 GridWidth = 0;
	int32 GridHeight = 0;
	float CellSize = 100.0f;
	FVector GridOrigin = FVector::ZeroVector;
	bool bGridInitialized = false;
	int32 RoadBudget = 0;
};
