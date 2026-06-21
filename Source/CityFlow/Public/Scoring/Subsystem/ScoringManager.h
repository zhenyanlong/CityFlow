#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Grid/CityFlowGridTypes.h"
#include "Scoring/Types/ScoringTypes.h"
#include "ScoringManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, NewTotalScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScorePopupRequested, FVector, WorldLocation, int32, DeltaScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationEvaluation);

/**
 * Collects live simulation events and converts them into a final planning report.
 * Live score is intentionally lightweight feedback; the authoritative final score
 * is recomputed from connectivity, traffic outcomes, efficiency, budget, runtime,
 * and a narrowly clamped map-difficulty multiplier when scoring stops.
 */
UCLASS()
class CITYFLOW_API UScoringManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	/** Resets per-match state and subscribes to VehicleManager events. */
	void StartScoring();

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	/** Freezes event collection, computes the final breakdown, and broadcasts evaluation. */
	void StopScoring();

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	FString GetPhaseSummary() const;

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	void RecordVehicleArrival(class AVehicleActor* Vehicle);

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	void RecordCongestion(const TSet<FGridVector>& CongestedCells);

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetTotalScore() const { return TotalScore; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetArrivalCount() const { return TotalArrivalCount; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetArrivalScore() const { return ArrivalScoreTotal; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetCongestionPenalty() const { return CongestionPenaltyTotal; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetDeathPenalty() const { return DeathPenaltyTotal; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetDeathCount() const { return DeathCount; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	float GetElapsedSimulationTime() const { return ElapsedSimulationTime; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	FCityFlowScoreBreakdown GetScoreBreakdown() const { return ScoreBreakdown; }

	UPROPERTY(BlueprintAssignable, Category = "Scoring|Events")
	FOnScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "Scoring|Events")
	FOnScorePopupRequested OnScorePopupRequested;

	UPROPERTY(BlueprintAssignable, Category = "Scoring|Events")
	FOnSimulationEvaluation OnSimulationEvaluation;

private:
	UFUNCTION()
	void OnVehicleArrivedHandler(class AVehicleActor* Vehicle);

	UFUNCTION()
	void OnCongestionUpdatedHandler();

	UFUNCTION()
	void OnVehicleDeathHandler(class AVehicleActor* Vehicle);

	UFUNCTION()
	void OnVehicleSpawnedHandler(class AVehicleActor* Vehicle);

	void BindVehicleDeathEvent(class AVehicleActor* Vehicle);
	void UnbindVehicleDeathEvent(class AVehicleActor* Vehicle);
	void RequestScorePopup(class AVehicleActor* Vehicle, int32 DeltaScore);

	void UpdateCongestionPenalty();

	void UpdateLiveScore();
	void ResetScoreState();
	void ComputeFinalScore();
	void ComputeConnectivityStats(FCityFlowScoreBreakdown& OutBreakdown) const;
	void ComputeTrafficStats(FCityFlowScoreBreakdown& OutBreakdown) const;
	void ComputeBudgetStats(FCityFlowScoreBreakdown& OutBreakdown) const;
	void ComputeDifficultyMultiplier(FCityFlowScoreBreakdown& OutBreakdown) const;

	TArray<class ABuilding*> GetAllBuildings() const;
	bool IsBuildingConnected(class ABuilding* Building) const;
	void BuildRoadComponentMap(TMap<FGridVector, int32>& OutComponentByCell) const;
	int32 EstimateMinimumRoadNeed(const TArray<class ABuilding*>& Buildings) const;

	class UVehicleManager* GetVehicleManager() const;
	class UGridManager* GetGridManager() const;

	int32 TotalScore = 0;
	int32 ArrivalScoreTotal = 0;
	int32 CongestionPenaltyTotal = 0;
	int32 DeathPenaltyTotal = 0;
	int32 DeathCount = 0;
	int32 SpawnedVehicleCount = 0;
	int32 TotalArrivalCount = 0;
	float ElapsedSimulationTime = 0.0f;
	int32 FinalCongestionCellCount = 0;
	float TotalTravelTimeOfArrivedVehicles = 0.0f;
	int32 TotalCellsTraversedByArrivedVehicles = 0;
	float SpawnedVehicleMoveSpeedTotal = 0.0f;

	bool bIsScoring = false;
	float CongestionUpdateTimer = 0.0f;
	TSet<FGridVector> CurrentCongestedCells;
	TSet<TObjectKey<class AVehicleActor>> ScoredDeathVehicles;
	FCityFlowScoreBreakdown ScoreBreakdown;

	FTimerHandle PenaltyTimerHandle;
	static constexpr float PENALTY_CHECK_INTERVAL = 1.0f;
};
