#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BuildingDataAsset.generated.h"

class ABuilding;

USTRUCT(BlueprintType)
struct CITYFLOW_API FBuildingDataEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	TSubclassOf<ABuilding> BuildingClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building", meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;
};

UCLASS(BlueprintType)
class CITYFLOW_API UBuildingDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Weighted building classes. Each building Blueprint owns its origin/destination role. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TArray<FBuildingDataEntry> BuildingEntries;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BuildingData"), GetFName());
	}
};
