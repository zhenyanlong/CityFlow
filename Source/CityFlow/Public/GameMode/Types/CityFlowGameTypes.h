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

UENUM(BlueprintType)
enum class ECityFlowDrivingSide : uint8
{
	RightHand	UMETA(DisplayName = "右舵 (靠右行驶)"),
	LeftHand	UMETA(DisplayName = "左舵 (靠左行驶)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGamePhaseChanged, ECityFlowGamePhase, OldPhase, ECityFlowGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlanningPhaseEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationPhaseEnd);
