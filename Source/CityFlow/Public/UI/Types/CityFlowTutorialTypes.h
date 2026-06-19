#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CityFlowTutorialTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowTutorialEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true), Category = "Tutorial")
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	TSoftObjectPtr<UTexture2D> Image;
};

/** Localizable tutorial content configured as a single reusable data asset. */
UCLASS(BlueprintType)
class CITYFLOW_API UCityFlowTutorialDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tutorial")
	TArray<FCityFlowTutorialEntry> Entries;
};
