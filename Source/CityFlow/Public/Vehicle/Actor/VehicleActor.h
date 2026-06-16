#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Vehicle/Types/CityFlowVehicleTypes.h"
#include "VehicleActor.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UWidgetComponent;
class ARoadTile;
class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;
class UMaterialInstanceDynamic;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleArrived, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleDeath, class AVehicleActor*, Vehicle);

UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API AVehicleActor : public AActor
{
	GENERATED_BODY()

public:
	AVehicleActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetSplinePath(const TArray<FVector>& WorldPoints, const TArray<FVector>& TangentDirs,
		const TArray<float>& ArriveTangentLengths, const TArray<float>& LeaveTangentLengths,
		float DefaultTangentLength = -1.0f);

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

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetPathCellCount(int32 InPathCellCount);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetPathCellCount() const { return PathCellCount; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetTravelTime() const { return TravelTime; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	FVector GetVelocityDirection() const { return VelocityDirection; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void SetDebugColor(FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Hover")
	void SetHovered(bool bInHovered);

	UFUNCTION(BlueprintPure, Category = "Vehicle|Hover")
	bool IsHovered() const { return bHovered; }

	/** Mark this intersection as passed — the vehicle can never re-acquire its lock. */
	void MarkIntersectionPassed(class ARoadTile* Tile);

	/** Returns true if this vehicle has already passed through (exited) the given intersection. */
	bool HasPassedIntersection(class ARoadTile* Tile) const;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleDeath OnVehicleDeath;

	/** Total stopped time (only accumulates when speed=0 and bFrontVehicleTooClose). Used for death timeout. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Death")
	float GetTotalStopTime() const { return TotalStopTime; }

	/** Time before timeout death is triggered (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death")
	float DeathTimeout = 5.0f;

	/** Stylized explosion VFX (Niagara). Configurable per-vehicle Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|VFX")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	/** Float value pushed to the Niagara User Parameter specified by ExplosionVFXScaleParamName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|VFX")
	float ExplosionVFXScale = 1.0f;

	/** Niagara User Parameter name to receive ExplosionVFXScale (e.g. "Scale"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|VFX")
	FName ExplosionVFXScaleParamName = TEXT("Scale");

	/** Explosion sound effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|SFX")
	TObjectPtr<USoundBase> ExplosionSFX;

	/** Camera shake class triggered on death. Proximity-scaled by distance from camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|Camera")
	TSubclassOf<UCameraShakeBase> DeathCameraShake;

	/** Max distance (cm) at which camera shake is still felt. Beyond this, shake=0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Death|Camera")
	float DeathShakeMaxDistance = 3000.0f;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleArrived OnVehicleArrived;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VehicleRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> PathSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> DestinationArrowWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Hover")
	bool bEnableHoverIndicator = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Hover", meta = (ClampMin = "0", ClampMax = "255"))
	int32 HoverStencilValue = 252;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Hover")
	float DestinationArrowHeight = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Hover")
	FRotator DestinationArrowRotationOffset = FRotator(-90.0f, 0.0f, 0.0f);

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

	/** Max time a vehicle can wait in WaitingCongestion before forcibly releasing all intersection reservations to break deadlocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
	float DeadlockTimeout = 3.0f;

	class ABuilding* Origin = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Death / Stop System (virtual — subclasses may override) =====

	/** Called every frame while vehicle speed reaches 0 (stuck in congestion).
	 *  Base: accumulates TotalStopTime, drives material flash effect.
	 */
	virtual void OnVehicleStopped(float DeltaTime);

	/** Called when vehicle resumes movement after being stopped.
	 *  Base: resets emissive flash on material.
	 */
	virtual void OnVehicleResumed();

	/** Triggers vehicle death. Base: plays VFX/SFX/CameraShake, broadcasts OnVehicleDeath, destroys actor.
	 *  Subclasses may override for custom death behavior.
	 */
	virtual void HandleVehicleDeath();

	/** Called when TotalStopTime exceeds DeathTimeout.
	 *  Base: calls HandleVehicleDeath() to destroy the vehicle.
	 *  Subclasses may override for custom timeout behavior (e.g., enter berserk mode).
	 */
	virtual void HandleWaitTimeout();

	/** Whether to reset TotalStopTime when vehicle resumes from stop.
	 *  Base: false (stop time accumulates continuously, leading to death).
	 *  Subclasses that don't die from waiting should return true.
	 */
	virtual bool ShouldResetStopTime() const { return false; }

	/** Speed multiplier applied while in berserk mode.
	 *  Base: 1.0. Subclasses may override (e.g., RampageVehicle returns 1.2).
	 */
	virtual float GetBerserkSpeedMultiplier() const { return 1.0f; }

	/** Builds a forward probe segment by sampling the current spline rather than actor transform. */
	bool BuildForwardProbeSegment(FVector& OutProbeStart, FVector& OutProbeEnd, FVector& OutDirection) const;

	/** Whether this vehicle is currently in berserk (rampage) mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Berserk")
	bool bBerserk = false;

	/** Kills all vehicles directly ahead in the movement direction. Called each frame when bBerserk is true. */
	void PerformRamKill();

	/** Kills all active vehicles overlapping the current actor location. Used by special vehicle subclasses. */
	void KillOverlappingVehicles(float OverlapRadius);

	/** Accumulated stopped time. Independent of CongestionWaitTime (deadlock release). */
	float TotalStopTime = 0.0f;

	/** Dynamic material instance for emissive pulsing during stop. Created lazily. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMaterialInstance;

	bool bHovered = false;

	void ApplyHoverRenderState(bool bInHovered);

	void RefreshDestinationArrowOffset();

	void UpdateDestinationArrow();

	void HandleArrival();

	void TickMovementSpline(float DeltaTime);

	EVehicleMovementState MovementState = EVehicleMovementState::Idle;
	FVector VelocityDirection = FVector::ZeroVector;
	float CurrentSplineDistance = 0.0f;
	float CurrentSpeed = 0.0f;
	float TravelTime = 0.0f;
	int32 PathCellCount = 0;

	FGridVector PreviousGridPosition;

	TObjectPtr<ABuilding> Destination;

	TWeakObjectPtr<AVehicleActor> FrontVehicle;
	float FrontVehicleDistance = 0.0f;
	bool bFrontVehicleTooClose = false;

	/** Accumulated time in WaitingCongestion state; used for deadlock detection. */
	float CongestionWaitTime = 0.0f;

	/** Intersections this vehicle has reserved via forward-probe. Tracked for cleanup on destruction/arrival. */
	TArray<TWeakObjectPtr<ARoadTile>> ReservedIntersections;

	/** Intersections this vehicle has physically passed through. Once passed, the vehicle
	 *  can never re-acquire the lock on the same intersection (prevents self-re-entry
	 *  after exit when the forward probe still sweeps the box just behind). */
	TSet<TWeakObjectPtr<ARoadTile>> PassedIntersections;

	void PerformForwardProbe();
};
