#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CityFlowRiverSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CityFlow River"))
class CITYFLOW_API UCityFlowRiverSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category = "Generation")
	bool bAutoGenerateOnNewGame = true;

	UPROPERTY(EditAnywhere, Config, Category = "Generation")
	int32 RandomSeed = 2401;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0", ClampMax = "4"))
	int32 RiverCount = 1;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0.25"))
	float RiverWidthCells = 1.25f;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0.0"))
	float BankWidthCells = 0.75f;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "2"))
	int32 SegmentCount = 8;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0.0"))
	float SinuosityCells = 3.0f;

	UPROPERTY(EditAnywhere, Config, Category = "Generation", meta = (ClampMin = "0"))
	int32 EdgeMarginCells = 2;

	UPROPERTY(EditAnywhere, Config, Category = "Render Target", meta = (ClampMin = "128"))
	int32 RenderTargetSize = 1024;

	UPROPERTY(EditAnywhere, Config, Category = "Render Target")
	FLinearColor ClearColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, Config, Category = "Render Target")
	FLinearColor BankMaskColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, Config, Category = "Render Target")
	FLinearColor WaterMaskColor = FLinearColor::White;
};
