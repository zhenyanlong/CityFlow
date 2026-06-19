#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CityFlowMenuSettingsSaveGame.generated.h"

UCLASS()
class CITYFLOW_API UCityFlowMenuSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	float SoundVolume = 1.0f;

	UPROPERTY(SaveGame)
	float SFXVolume = 1.0f;

	UPROPERTY(SaveGame)
	FString CultureCode;
};
