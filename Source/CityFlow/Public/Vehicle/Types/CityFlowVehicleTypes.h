#pragma once

#include "CoreMinimal.h"
#include "Grid/CityFlowGridTypes.h"
#include "CityFlowVehicleTypes.generated.h"

UENUM(BlueprintType)
enum class EVehicleMovementState : uint8
{
	Idle,
	Moving,
	WaitingCongestion,
	WaitingIntersection,
	Arrived
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FVehiclePathNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGridVector GridPos;

	UPROPERTY(BlueprintReadOnly)
	FVector WorldPos;

	UPROPERTY(BlueprintReadOnly)
	uint8 ConnectedMask = 0;

	FVehiclePathNode() = default;
	FVehiclePathNode(const FGridVector& InGridPos, const FVector& InWorldPos)
		: GridPos(InGridPos), WorldPos(InWorldPos) {}
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FVehicleWaypoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Position;

	UPROPERTY(BlueprintReadOnly)
	float Speed = 1.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bIsIntersectionEntry = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsIntersectionExit = false;

	FVehicleWaypoint() = default;
	FVehicleWaypoint(const FVector& InPos, float InSpeed = 1.0f)
		: Position(InPos), Speed(InSpeed) {}
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FVehicleMovementPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FVehicleWaypoint> Waypoints;

	int32 CurrentWaypointIndex = 0;

	bool IsComplete() const { return CurrentWaypointIndex >= Waypoints.Num(); }
	bool IsValid() const { return Waypoints.Num() > 0; }

	void Reset() { CurrentWaypointIndex = 0; }

	const FVehicleWaypoint* GetCurrentWaypoint() const
	{
		if (CurrentWaypointIndex < Waypoints.Num())
		{
			return &Waypoints[CurrentWaypointIndex];
		}
		return nullptr;
	}

	const FVehicleWaypoint* GetNextWaypoint() const
	{
		const int32 NextIdx = CurrentWaypointIndex + 1;
		if (NextIdx < Waypoints.Num())
		{
			return &Waypoints[NextIdx];
		}
		return nullptr;
	}

	void Advance()
	{
		++CurrentWaypointIndex;
	}
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FIntersectionLock
{
	GENERATED_BODY()

	FGridVector IntersectionPos;
	class AVehicleActor* OccupyingVehicle = nullptr;
	bool bOccupied = false;

	void Acquire(class AVehicleActor* Vehicle)
	{
		bOccupied = true;
		OccupyingVehicle = Vehicle;
	}

	void Release()
	{
		bOccupied = false;
		OccupyingVehicle = nullptr;
	}
};

USTRUCT()
struct CITYFLOW_API FAStarNode
{
	GENERATED_BODY()

	FGridVector Position;
	int32 GCost = 0;
	int32 HCost = 0;
	FGridVector Parent;
	bool bClosed = false;

	int32 FCost() const { return GCost + HCost; }

	bool operator<(const FAStarNode& Other) const
	{
		return FCost() > Other.FCost();
	}

	FAStarNode() = default;
	FAStarNode(const FGridVector& InPos, int32 InG, int32 InH, const FGridVector& InParent)
		: Position(InPos), GCost(InG), HCost(InH), Parent(InParent) {}
};
