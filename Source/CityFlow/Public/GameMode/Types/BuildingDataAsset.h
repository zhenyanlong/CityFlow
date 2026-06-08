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
	/** 可生成的建筑列表。是起点还是目的地由建筑蓝图自身的 bIsDestination 决定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	TArray<FBuildingDataEntry> BuildingEntries;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BuildingData"), GetFName());
	}
};
