#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Vehicle/Types/CityFlowVehicleTypes.h"
#include "GameMode/Types/CityFlowGameTypes.h"

class AVehicleActor;
class ABuilding;
class UGridManager;
class UVehicleDataAsset;
struct FVehicleSpawnEntry;

#include "VehicleManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleSpawned, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleArrivedAtDest, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleDied, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleAbilityActivated, class AVehicleActor*, Vehicle, EVehicleAbilityAlertType, AlertType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCongestionUpdated);

/**
 * Owns vehicle population, route construction, congestion sampling, and global
 * traffic events. The road graph is rebuilt after planning and treated as
 * read-only during Simulation so many vehicles can share topology safely.
 */
UCLASS()
class CITYFLOW_API UVehicleManager : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVehicleManager, STATGROUP_Tickables); }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void StopSpawning();

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ClearAllVehicles();

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	bool IsSpawning() const { return bIsActive; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetDrivingSide(ECityFlowDrivingSide Side) { DrivingSide = Side; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetLaneOffsetFactor(float Factor) { LaneOffsetFactor = FMath::Clamp(Factor, 0.0f, 0.45f); }

	/** Configures a target-population refill policy for the next simulation. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Spawning")
	void ConfigureSpawnProfile(float InSpawnInterval, int32 InTargetActiveVehicles,
		int32 InMaxActiveVehicles, int32 InSpawnBurstSize);

	UFUNCTION(BlueprintPure, Category = "Vehicle|Spawning")
	float GetSpawnInterval() const { return SpawnInterval; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|Spawning")
	int32 GetSpawnBurstSize() const { return SpawnBurstSize; }

	/** Sets a match-specific vehicle data asset that overrides DeveloperSettings. */
	void SetVehicleDataAsset(UVehicleDataAsset* InDataAsset) { ExternalVehicleDataAsset = InDataAsset; }

	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	AVehicleActor* SpawnVehicle(class ABuilding* Origin, class ABuilding* Destination);

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool BuildPath(const FGridVector& Start, const FGridVector& End, TArray<FGridVector>& OutPath) const;

	TArray<FVector> BuildSplinePath(const TArray<FGridVector>& Path,
		TArray<FVector>& OutTangentDirs,
		TArray<float>& OutArriveTangentLengths,
		TArray<float>& OutLeaveTangentLengths) const;

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	const TArray<AVehicleActor*>& GetActiveVehicles() const { return ActiveVehicles; }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetActiveVehicleCount() const { return ActiveVehicles.Num(); }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetArrivedVehicleCount() const { return ArrivedVehicles.Num(); }

	UFUNCTION(BlueprintPure, Category = "Vehicle")
	const TSet<FGridVector>& GetCongestedCells() const { return CongestedCells; }

	void NotifyVehicleAbilityActivated(AVehicleActor* Vehicle, EVehicleAbilityAlertType AlertType);

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void DebugDrawPaths() const;

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void DebugDrawIntersections() const;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleSpawned OnVehicleSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleArrivedAtDest OnVehicleArrived;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleDied OnVehicleDied;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleAbilityActivated OnVehicleAbilityActivated;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnCongestionUpdated OnCongestionUpdated;

private:
	class UGridManager* GetGridManager() const;

	void CollectOriginDestinations(TArray<class ABuilding*>& OutOrigins, TArray<class ABuilding*>& OutDestinations) const;

	TArray<FGridVector> FindRoadPath(const FGridVector& Start, const FGridVector& End) const;
	void UpdateCongestion();
	void SanitizeAllIntersectionLocks();

	UFUNCTION()
	void OnVehicleDeathHandler(AVehicleActor* Vehicle);

	bool IsIntersection(const FGridVector& Pos) const;
	bool IsBuildingBlocked(class ABuilding* Building) const;

	static int32 ManhattanDist(const FGridVector& A, const FGridVector& B);

	TSubclassOf<AVehicleActor> PickRandomVehicleClass() const;
	void CacheSpawnEntries();

	UPROPERTY()
	TArray<AVehicleActor*> ActiveVehicles;

	UPROPERTY()
	TArray<AVehicleActor*> ArrivedVehicles;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle CongestionTimerHandle;
	FTimerHandle SanitizeTimerHandle;
	bool bIsActive = false;
	float TimeSinceLastSpawn = 0.0f;
	float SpawnInterval = 5.0f;
	int32 TargetActiveVehicleCount = 24;
	int32 MaxActiveVehicleCount = 40;
	int32 SpawnBurstSize = 3;
	bool bHasSpawnProfileOverride = false;

	TSet<FGridVector> CongestedCells;

	TArray<FVehicleSpawnEntry> CachedSpawnEntries;
	float TotalSpawnWeight = 0.0f;

	ECityFlowDrivingSide DrivingSide = ECityFlowDrivingSide::RightHand;
	float LaneOffsetFactor = 0.2f;

	TObjectPtr<UVehicleDataAsset> ExternalVehicleDataAsset;

	static constexpr float CONGESTION_CHECK_INTERVAL = 1.0f;
};
