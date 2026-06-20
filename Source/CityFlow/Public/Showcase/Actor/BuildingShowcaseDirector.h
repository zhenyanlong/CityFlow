#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/CityFlowGridTypes.h"
#include "BuildingShowcaseDirector.generated.h"

class ABuilding;
class AGridVisualizer;
class UBuildingDataAsset;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBuildingShowcaseBuilt, int32, PlacedBuildingCount, int32, FailedBuildingCount);

/**
 * Initializes a real CityFlow grid and places one instance of every configured
 * building class through AGridPlaceableActor::PlaceOnGrid().
 */
UCLASS(Blueprintable)
class CITYFLOW_API ABuildingShowcaseDirector : public AActor
{
	GENERATED_BODY()

public:
	ABuildingShowcaseDirector();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Showcase")
	void BuildShowcase();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Showcase")
	void ClearShowcase();

	UFUNCTION(BlueprintPure, Category = "CityFlow|Showcase")
	TArray<ABuilding*> GetSpawnedBuildings() const;

	UPROPERTY(BlueprintAssignable, Category = "CityFlow|Showcase")
	FOnBuildingShowcaseBuilt OnShowcaseBuilt;

	/** Existing weighted building list used by normal matches; weights are ignored here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Content")
	TObjectPtr<UBuildingDataAsset> BuildingDataAsset;

	/** Optional extra classes appended after the DataAsset entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Content")
	TArray<TSubclassOf<ABuilding>> AdditionalBuildingClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Content")
	bool bDeduplicateBuildingClasses = true;

	/** Creates Rot0/90/180/270 copies for every class instead of one copy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Content")
	bool bSpawnAllRotations = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Content", meta = (EditCondition = "!bSpawnAllRotations"))
	EGridRotation SingleRotation = EGridRotation::Rot0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Grid", meta = (ClampMin = "4"))
	int32 GridWidth = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Grid", meta = (ClampMin = "4"))
	int32 GridHeight = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Grid", meta = (ClampMin = "10.0"))
	float CellSize = 200.0f;

	/** Added to this Actor's location to produce GridManager's cell (0,0) center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Grid")
	FVector GridOriginOffset = FVector::ZeroVector;

	/** Empty cells between buildings and around the outer grid edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Layout", meta = (ClampMin = "1"))
	int32 PaddingCells = 2;

	/** Expands height (and minimum width) so every configured class can be shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Layout")
	bool bAutoExpandGridToFit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Presentation")
	bool bPlaySpawnAnimations = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Presentation")
	bool bSpawnGridVisualizer = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase|Presentation", meta = (EditCondition = "bSpawnGridVisualizer"))
	TSubclassOf<AGridVisualizer> GridVisualizerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CityFlow|Showcase")
	bool bBuildOnBeginPlay = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABuilding>> SpawnedBuildings;

	UPROPERTY(Transient)
	TObjectPtr<AGridVisualizer> SpawnedGridVisualizer;
};
