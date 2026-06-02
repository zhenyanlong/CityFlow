#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
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
	void SetSplinePath(const TArray<FVector>& WorldPoints);

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetDestination(class ABuilding* InDestination);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	class ABuilding* GetDestination() const { return Destination; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	EVehicleMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSplineDistance() const { return CurrentSplineDistance; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	class USplineComponent* GetPathSpline() const { return PathSpline; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> PathSpline;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float WaypointReachedThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float IntersectionWaitTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float VehicleZOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Debug")
	FLinearColor DebugColor = FLinearColor(0.8f, 0.2f, 0.2f, 1.0f);

	void SetWaitingForIntersection(bool bWaiting);
	bool IsWaitingForIntersection() const { return MovementState == EVehicleMovementState::WaitingIntersection; }

	class ABuilding* Origin = nullptr;

protected:
	virtual void BeginPlay() override;

private:
	void TickMovementSpline(float DeltaTime);
	void HandleArrival();

	EVehicleMovementState MovementState = EVehicleMovementState::Idle;
	FVector VelocityDirection = FVector::ZeroVector;
	float CurrentSplineDistance = 0.0f;

	TObjectPtr<ABuilding> Destination;

	FGridVector WaitingIntersectionPos;
	float IntersectionWaitTimer = 0.0f;
};
