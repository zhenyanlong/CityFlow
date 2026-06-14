#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Environment/Types/CityFlowLandscapeDecorationTypes.h"
#include "CityFlowLandscapeDecorationSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CityFlow Landscape Decoration"))
class CITYFLOW_API UCityFlowLandscapeDecorationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category = "Generation")
	bool bAutoGenerateOnNewGame = true;

	UPROPERTY(EditAnywhere, Config, Category = "Generation")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0"))
	int32 InstancesPerCell = 1;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0"))
	int32 PlacementClearPaddingCells = 0;

	UPROPERTY(EditAnywhere, Config, Category = "Generation")
	TArray<FCityFlowLandscapeDecorationConfig> Decorations;

	UPROPERTY(EditAnywhere, Config, Category = "Grass Coverage")
	FCityFlowGrassCoverageConfig GrassCoverage;
};
