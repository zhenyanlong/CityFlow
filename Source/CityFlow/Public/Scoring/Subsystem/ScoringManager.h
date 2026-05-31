#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Grid/CityFlowGridTypes.h"
#include "ScoringManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreChanged, int32, NewTotalScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSimulationEvaluation);

UCLASS()
class CITYFLOW_API UScoringManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	void StartScoring();

	UFUNCTION(BlueprintCallable, Category = "Scoring")
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
	float GetElapsedSimulationTime() const { return ElapsedSimulationTime; }

	UPROPERTY(BlueprintAssignable, Category = "Scoring|Events")
	FOnScoreChanged OnScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "Scoring|Events")
	FOnSimulationEvaluation OnSimulationEvaluation;

private:
	UFUNCTION()
	void OnVehicleArrivedHandler(class AVehicleActor* Vehicle);

	UFUNCTION()
	void OnCongestionUpdatedHandler();

	void UpdateCongestionPenalty();

	void ComputeFinalScore(bool bAllConnected);

	class UVehicleManager* GetVehicleManager() const;
	class UGridManager* GetGridManager() const;

	int32 TotalScore = 0;
	int32 ArrivalScoreTotal = 0;
	int32 CongestionPenaltyTotal = 0;
	int32 TotalArrivalCount = 0;
	float ElapsedSimulationTime = 0.0f;
	int32 FinalCongestionCellCount = 0;

	bool bIsScoring = false;
	float CongestionUpdateTimer = 0.0f;
	TSet<FGridVector> CurrentCongestedCells;

	FTimerHandle PenaltyTimerHandle;
	static constexpr float PENALTY_CHECK_INTERVAL = 1.0f;
};
