#pragma once

#include "CoreMinimal.h"
#include "CityFlowGameTypes.generated.h"

UENUM(BlueprintType)
enum class ECityFlowGamePhase : uint8
{
	None,
	Planning,
	Simulating,
	Evaluation
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePhaseChanged, ECityFlowGamePhase, OldPhase, ECityFlowGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlanningPhaseEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationPhaseEnd);
