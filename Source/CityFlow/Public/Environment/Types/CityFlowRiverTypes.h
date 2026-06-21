#pragma once

#include "CoreMinimal.h"
#include "Grid/CityFlowGridTypes.h"
#include "CityFlowRiverTypes.generated.h"

/** One generated centre line plus its rasterised river and bank occupancy cells. */
USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowRiverPath
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "River")
	TArray<FVector> Points;

	UPROPERTY(BlueprintReadOnly, Category = "River")
	TArray<FGridVector> RiverCells;

	UPROPERTY(BlueprintReadOnly, Category = "River")
	TArray<FGridVector> BankCells;
};
