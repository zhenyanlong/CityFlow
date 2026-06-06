#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Vehicle/Types/CityFlowVehicleTypes.h"
#include "GameMode/Types/CityFlowGameTypes.h"

class AVehicleActor;
class ABuilding;
class UGridManager;
struct FVehicleSpawnEntry;

#include "VehicleManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleSpawned, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleArrivedAtDest, class AVehicleActor*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCongestionUpdated);

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

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void DebugDrawPaths() const;

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Debug")
	void DebugDrawIntersections() const;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleSpawned OnVehicleSpawned;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnVehicleArrivedAtDest OnVehicleArrived;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FOnCongestionUpdated OnCongestionUpdated;

private:
	class UGridManager* GetGridManager() const;

	void CollectOriginDestinations(TArray<class ABuilding*>& OutOrigins, TArray<class ABuilding*>& OutDestinations) const;

	TArray<FGridVector> FindRoadPath(const FGridVector& Start, const FGridVector& End) const;
	void UpdateCongestion();
	void SanitizeAllIntersectionLocks();

	bool IsIntersection(const FGridVector& Pos) const;
	bool IsOccupiedByVehicle(const FGridVector& GridPos) const;
	bool IsBuildingBlocked(class ABuilding* Building) const;
	void UpdateVehicleGridOccupancy(AVehicleActor* Vehicle);

	static int32 ManhattanDist(const FGridVector& A, const FGridVector& B);

	TSubclassOf<AVehicleActor> PickRandomVehicleClass() const;
	void CacheSpawnEntries();

	UPROPERTY()
	TArray<AVehicleActor*> ActiveVehicles;

	UPROPERTY()
	TArray<AVehicleActor*> ArrivedVehicles;

	UPROPERTY()
	TMap<FGridVector, AVehicleActor*> VehicleGridMap;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle CongestionTimerHandle;
	FTimerHandle SanitizeTimerHandle;
	bool bIsActive = false;
	float TimeSinceLastSpawn = 0.0f;
	float SpawnInterval = 5.0f;

	TSet<FGridVector> CongestedCells;

	TArray<FVehicleSpawnEntry> CachedSpawnEntries;
	float TotalSpawnWeight = 0.0f;

	ECityFlowDrivingSide DrivingSide = ECityFlowDrivingSide::RightHand;
	float LaneOffsetFactor = 0.2f;

	static constexpr float CONGESTION_CHECK_INTERVAL = 1.0f;
};
