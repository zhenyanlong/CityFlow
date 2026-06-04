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
	void SetSplinePath(const TArray<FVector>& WorldPoints, const TArray<FVector>& TangentDirs,
		const TArray<float>& ArriveTangentLengths, const TArray<float>& LeaveTangentLengths,
		float DefaultTangentLength = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetDestination(class ABuilding* InDestination);

	/** Feed the A* grid path so the vehicle knows which intersection cells it will cross. */
	void SetPathIntersections(const TArray<FGridVector>& GridPath);

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
	float GetCurrentSpeed() const { return CurrentSpeed; }

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
	float Acceleration = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float DecelerationDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float WaypointReachedThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float IntersectionWaitTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float VehicleZOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float ForwardProbeRadius = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float ForwardProbeDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float SelfAvoidOffset = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float ProbeVerticalOffset = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float SafeDistanceMin = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float SafeDistanceSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float StartAcceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float StartDeceleration = 2500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Debug")
	FLinearColor DebugColor = FLinearColor(0.8f, 0.2f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Debug")
	bool bDebugDrawProbe = false;

	void SetWaitingForIntersection(bool bWaiting);

	class ABuilding* Origin = nullptr;

protected:
	virtual void BeginPlay() override;

private:
	void TickMovementSpline(float DeltaTime);
	void HandleArrival();

	EVehicleMovementState MovementState = EVehicleMovementState::Idle;
	FVector VelocityDirection = FVector::ZeroVector;
	float CurrentSplineDistance = 0.0f;
	float CurrentSpeed = 0.0f;

	FGridVector PreviousGridPosition;

	TObjectPtr<ABuilding> Destination;

	TWeakObjectPtr<AVehicleActor> FrontVehicle;
	float FrontVehicleDistance = 0.0f;
	bool bFrontVehicleTooClose = false;

	TArray<FGridVector> PathIntersectionCells;

	void PerformForwardProbe();
};
