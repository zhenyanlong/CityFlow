#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VehicleDataAsset.generated.h"

class AVehicleActor;

/** Weighted class entry used by VehicleManager's deterministic spawn selection. */
USTRUCT(BlueprintType)
struct CITYFLOW_API FVehicleSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	TSubclassOf<AVehicleActor> VehicleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle", meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;
};

/** Data-driven vehicle roster that avoids hard-coding Blueprint classes in C++. */
UCLASS(BlueprintType)
class CITYFLOW_API UVehicleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TArray<FVehicleSpawnEntry> VehicleEntries;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("VehicleData"), GetFName());
	}
};
