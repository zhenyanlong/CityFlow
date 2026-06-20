#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CityFlowShowcaseGameMode.generated.h"

/** Lightweight level override that prevents the normal CityFlow HUD/menu preview from starting. */
UCLASS(Blueprintable)
class CITYFLOW_API ACityFlowShowcaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACityFlowShowcaseGameMode();
};
