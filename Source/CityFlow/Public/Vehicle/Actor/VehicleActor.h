#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Vehicle/Types/CityFlowVehicleTypes.h"
#include "VehicleActor.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleArrived, class AVehicleActor*, Vehicle);

UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API AVehicleActor : public AActor
{
	GENERATED_BODY()

public:
	AVehicleActor();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetMovementPlan(const FVehicleMovementPlan& Plan);

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetDestination(class ABuilding* InDestination);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	class ABuilding* GetDestination() const { return Destination; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	EVehicleMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	const FVehicleMovementPlan& GetMovementPlan() const { return MovementPlan; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetCurrentWaypointIndex() const { return MovementPlan.CurrentWaypointIndex; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetMoveSpeed() const { return MoveSpeed; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	FVector GetVelocityDirection() const { return VelocityDirection; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void SetDebugColor(FLinearColor Color);

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleArrived OnVehicleArrived;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VehicleRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VehicleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float MinFollowDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float WaypointReachedThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float IntersectionWaitTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float VehicleZOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Debug")
	FLinearColor DebugColor = FLinearColor(0.8f, 0.2f, 0.2f, 1.0f);

	void AssignPathId(int32 PathId) { AssignedPathId = PathId; }
	int32 GetPathId() const { return AssignedPathId; }

	void SetWaitingForIntersection(bool bWaiting);
	bool IsWaitingForIntersection() const { return MovementState == EVehicleMovementState::WaitingIntersection; }

	bool NeedsIntersectionLock(const FGridVector& IntersectionPos) const;
	void NotifyIntersectionCleared(const FGridVector& IntersectionPos);

	class ABuilding* Origin = nullptr;

protected:
	virtual void BeginPlay() override;

private:
	void TickMovementSimple(float DeltaTime);
	void HandleArrival();

	FVehicleMovementPlan MovementPlan;
	EVehicleMovementState MovementState = EVehicleMovementState::Idle;
	FVector VelocityDirection = FVector::ZeroVector;

	TObjectPtr<ABuilding> Destination;
	int32 AssignedPathId = -1;

	FGridVector WaitingIntersectionPos;
	float IntersectionWaitTimer = 0.0f;
};
