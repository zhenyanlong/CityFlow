#include "Scoring/Subsystem/ScoringManager.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Grid/GridManager.h"
#include "Grid/Building.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogScoring, Log, All);

void UScoringManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UScoringManager::Deinitialize()
{
	StopScoring();
	Super::Deinitialize();
}

void UScoringManager::StartScoring()
{
	StopScoring();

	ResetScoreState();

	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->OnVehicleArrived.AddDynamic(this, &UScoringManager::OnVehicleArrivedHandler);
		VM->OnCongestionUpdated.AddDynamic(this, &UScoringManager::OnCongestionUpdatedHandler);
		VM->OnVehicleDied.AddDynamic(this, &UScoringManager::OnVehicleDeathHandler);
		VM->OnVehicleSpawned.AddDynamic(this, &UScoringManager::OnVehicleSpawnedHandler);

		for (AVehicleActor* Vehicle : VM->GetActiveVehicles())
		{
			BindVehicleDeathEvent(Vehicle);
		}
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			PenaltyTimerHandle,
			this,
			&UScoringManager::UpdateCongestionPenalty,
			PENALTY_CHECK_INTERVAL,
			true,
			0.0f
		);
	}

	bIsScoring = true;
}

void UScoringManager::StopScoring()
{
	const bool bWasScoring = bIsScoring;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PenaltyTimerHandle);
	}

	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->OnVehicleArrived.RemoveDynamic(this, &UScoringManager::OnVehicleArrivedHandler);
		VM->OnCongestionUpdated.RemoveDynamic(this, &UScoringManager::OnCongestionUpdatedHandler);
		VM->OnVehicleDied.RemoveDynamic(this, &UScoringManager::OnVehicleDeathHandler);
		VM->OnVehicleSpawned.RemoveDynamic(this, &UScoringManager::OnVehicleSpawnedHandler);

		for (AVehicleActor* Vehicle : VM->GetActiveVehicles())
		{
			UnbindVehicleDeathEvent(Vehicle);
		}
	}

	if (bWasScoring)
	{
		ComputeFinalScore();
	}

	bIsScoring = false;
	if (bWasScoring)
	{
		OnSimulationEvaluation.Broadcast();
	}
}

FString UScoringManager::GetPhaseSummary() const
{
	return FString::Printf(TEXT("Score: %d | Raw: %.1f | Connectivity: %.1f | Traffic: %.1f | Efficiency: %.1f | Budget: %.1f | Runtime: %.1f | Multiplier: %.2f"),
		TotalScore,
		ScoreBreakdown.RawScore,
		ScoreBreakdown.ConnectivityScore,
		ScoreBreakdown.TrafficOutcomeScore,
		ScoreBreakdown.TravelEfficiencyScore,
		ScoreBreakdown.BudgetEfficiencyScore,
		ScoreBreakdown.RuntimeScore,
		ScoreBreakdown.MapDifficultyMultiplier);
}

void UScoringManager::RecordVehicleArrival(AVehicleActor* Vehicle)
{
	if (!Vehicle || !bIsScoring)
	{
		return;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const int32 ArrivalPoints = Settings ? Settings->ArrivalScore : 100;

	++TotalArrivalCount;
	ArrivalScoreTotal += ArrivalPoints;
	TotalTravelTimeOfArrivedVehicles += Vehicle->GetTravelTime();
	TotalCellsTraversedByArrivedVehicles += FMath::Max(1, Vehicle->GetPathCellCount());

	RequestScorePopup(Vehicle, ArrivalPoints);
}

void UScoringManager::RecordCongestion(const TSet<FGridVector>& CongestedCells)
{
	CurrentCongestedCells = CongestedCells;
}

void UScoringManager::OnVehicleArrivedHandler(AVehicleActor* Vehicle)
{
	RecordVehicleArrival(Vehicle);
}

void UScoringManager::OnVehicleDeathHandler(AVehicleActor* Vehicle)
{
	if (!Vehicle || !bIsScoring)
	{
		return;
	}

	const TObjectKey<AVehicleActor> VehicleKey(Vehicle);
	if (ScoredDeathVehicles.Contains(VehicleKey))
	{
		return;
	}
	ScoredDeathVehicles.Add(VehicleKey);

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const int32 Penalty = Settings ? Settings->DeathPenalty : 50;

	++DeathCount;
	DeathPenaltyTotal += Penalty;

	RequestScorePopup(Vehicle, -Penalty);

	UnbindVehicleDeathEvent(Vehicle);
}

void UScoringManager::OnVehicleSpawnedHandler(AVehicleActor* Vehicle)
{
	if (Vehicle)
	{
		++SpawnedVehicleCount;
		SpawnedVehicleMoveSpeedTotal += Vehicle->GetMoveSpeed();
	}

	BindVehicleDeathEvent(Vehicle);
}

void UScoringManager::BindVehicleDeathEvent(AVehicleActor* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}

	Vehicle->OnVehicleDeath.RemoveDynamic(this, &UScoringManager::OnVehicleDeathHandler);
	Vehicle->OnVehicleDeath.AddDynamic(this, &UScoringManager::OnVehicleDeathHandler);
}

void UScoringManager::UnbindVehicleDeathEvent(AVehicleActor* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}

	Vehicle->OnVehicleDeath.RemoveDynamic(this, &UScoringManager::OnVehicleDeathHandler);
}

void UScoringManager::RequestScorePopup(AVehicleActor* Vehicle, int32 DeltaScore)
{
	if (!Vehicle)
	{
		return;
	}

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (Settings && !Settings->bEnableScorePopups)
	{
		return;
	}

	OnScorePopupRequested.Broadcast(Vehicle->GetActorLocation(), DeltaScore);
}

void UScoringManager::OnCongestionUpdatedHandler()
{
	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		CurrentCongestedCells = VM->GetCongestedCells();
	}
}

void UScoringManager::UpdateCongestionPenalty()
{
	if (!bIsScoring)
	{
		return;
	}

	ElapsedSimulationTime += PENALTY_CHECK_INTERVAL;

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (!Settings)
	{
		return;
	}

	const int32 CongestedCount = CurrentCongestedCells.Num();
	if (CongestedCount > 0)
	{
		const int32 Penalty = CongestedCount * Settings->CongestionPenaltyPerSecond;
		CongestionPenaltyTotal += Penalty;
		FinalCongestionCellCount = FMath::Max(FinalCongestionCellCount, CongestedCount);
	}

}

void UScoringManager::ResetScoreState()
{
	TotalScore = 0;
	ArrivalScoreTotal = 0;
	CongestionPenaltyTotal = 0;
	DeathPenaltyTotal = 0;
	DeathCount = 0;
	SpawnedVehicleCount = 0;
	TotalArrivalCount = 0;
	ElapsedSimulationTime = 0.0f;
	CurrentCongestedCells.Empty();
	ScoredDeathVehicles.Empty();
	FinalCongestionCellCount = 0;
	TotalTravelTimeOfArrivedVehicles = 0.0f;
	TotalCellsTraversedByArrivedVehicles = 0;
	SpawnedVehicleMoveSpeedTotal = 0.0f;
	ScoreBreakdown = FCityFlowScoreBreakdown();
}

void UScoringManager::ComputeFinalScore()
{
	FCityFlowScoreBreakdown NewBreakdown;
	NewBreakdown.ElapsedSimulationTime = ElapsedSimulationTime;

	ComputeConnectivityStats(NewBreakdown);
	ComputeTrafficStats(NewBreakdown);
	ComputeBudgetStats(NewBreakdown);

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const float AcceptableMultiplier = Settings ? Settings->AcceptableCellTimeMultiplier : 2.5f;
	const float AverageMoveSpeed = SpawnedVehicleCount > 0
		? SpawnedVehicleMoveSpeedTotal / static_cast<float>(SpawnedVehicleCount)
		: 600.0f;

	UGridManager* GM = GetGridManager();
	const float CellSize = GM ? GM->GetCellSize() : 200.0f;
	const float IdealCellTime = AverageMoveSpeed > KINDA_SMALL_NUMBER ? CellSize / AverageMoveSpeed : 0.0f;
	const float AcceptableCellTime = IdealCellTime * AcceptableMultiplier;

	if (NewBreakdown.TotalCellsTraversedByArrivedVehicles > 0)
	{
		NewBreakdown.AverageCellTravelTime =
			NewBreakdown.TotalTravelTimeOfArrivedVehicles / static_cast<float>(NewBreakdown.TotalCellsTraversedByArrivedVehicles);
	}

	if (TotalArrivalCount > 0 && AcceptableCellTime > IdealCellTime)
	{
		const float EfficiencyRatio = FMath::Clamp(
			(AcceptableCellTime - NewBreakdown.AverageCellTravelTime) / (AcceptableCellTime - IdealCellTime),
			0.0f,
			1.0f);
		NewBreakdown.TravelEfficiencyScore = 200.0f * EfficiencyRatio;
	}

	const float CompletionRatio = NewBreakdown.ArrivalRate * (1.0f - NewBreakdown.DeathRate);
	const float SimulationDuration = Settings ? Settings->SimulationDurationSeconds : 180.0f;
	const float VehicleSpawnInterval = Settings ? Settings->VehicleSpawnInterval : 5.0f;
	const float ExpectedSpawnedVehicles = VehicleSpawnInterval > KINDA_SMALL_NUMBER
		? SimulationDuration / VehicleSpawnInterval
		: 0.0f;

	if (ExpectedSpawnedVehicles > 0.0f
		&& NewBreakdown.SpawnedVehicles >= ExpectedSpawnedVehicles * 0.5f
		&& SimulationDuration > KINDA_SMALL_NUMBER)
	{
		const float TimeRatio = FMath::Clamp(1.0f - ElapsedSimulationTime / SimulationDuration, 0.0f, 1.0f);
		NewBreakdown.RuntimeScore = 100.0f * TimeRatio * CompletionRatio;
	}

	NewBreakdown.RawScore =
		NewBreakdown.ConnectivityScore
		+ NewBreakdown.TrafficOutcomeScore
		+ NewBreakdown.TravelEfficiencyScore
		+ NewBreakdown.BudgetEfficiencyScore
		+ NewBreakdown.RuntimeScore;

	ComputeDifficultyMultiplier(NewBreakdown);

	NewBreakdown.FinalScore = FMath::RoundToInt(NewBreakdown.RawScore * NewBreakdown.MapDifficultyMultiplier);
	ScoreBreakdown = NewBreakdown;
	TotalScore = ScoreBreakdown.FinalScore;

	OnScoreChanged.Broadcast(TotalScore);
}

void UScoringManager::ComputeConnectivityStats(FCityFlowScoreBreakdown& OutBreakdown) const
{
	const TArray<ABuilding*> Buildings = GetAllBuildings();
	OutBreakdown.TotalBuildingCount = Buildings.Num();
	if (Buildings.Num() == 0)
	{
		OutBreakdown.bAllConnected = true;
		return;
	}

	TMap<FGridVector, int32> ComponentByCell;
	BuildRoadComponentMap(ComponentByCell);

	TMap<int32, int32> BuildingCountByComponent;
	for (ABuilding* Building : Buildings)
	{
		if (!Building || !IsBuildingConnected(Building))
		{
			continue;
		}

		++OutBreakdown.ConnectedBuildingCount;

		TSet<int32> BuildingComponents;
		for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
		{
			if (const int32* ComponentId = ComponentByCell.Find(DoorwayPos))
			{
				BuildingComponents.Add(*ComponentId);
			}
		}

		for (const int32 ComponentId : BuildingComponents)
		{
			int32& Count = BuildingCountByComponent.FindOrAdd(ComponentId);
			++Count;
			OutBreakdown.LargestConnectedBuildingComponent =
				FMath::Max(OutBreakdown.LargestConnectedBuildingComponent, Count);
		}
	}

	OutBreakdown.bAllConnected = OutBreakdown.ConnectedBuildingCount == OutBreakdown.TotalBuildingCount;

	const float TotalBuildings = static_cast<float>(OutBreakdown.TotalBuildingCount);
	const float ConnectedRatio = OutBreakdown.ConnectedBuildingCount / TotalBuildings;
	const float LargestComponentRatio = OutBreakdown.LargestConnectedBuildingComponent / TotalBuildings;
	OutBreakdown.ConnectivityScore =
		180.0f * FMath::Square(ConnectedRatio)
		+ 80.0f * LargestComponentRatio
		+ (OutBreakdown.bAllConnected ? 40.0f : 0.0f);
}

void UScoringManager::ComputeTrafficStats(FCityFlowScoreBreakdown& OutBreakdown) const
{
	const UVehicleManager* VM = GetVehicleManager();

	OutBreakdown.ArrivedVehicles = TotalArrivalCount;
	OutBreakdown.DeadVehicles = DeathCount;
	OutBreakdown.ActiveVehiclesAtEnd = VM ? VM->GetActiveVehicleCount() : 0;
	OutBreakdown.SpawnedVehicles = FMath::Max(
		SpawnedVehicleCount,
		OutBreakdown.ArrivedVehicles + OutBreakdown.DeadVehicles + OutBreakdown.ActiveVehiclesAtEnd);

	const float Spawned = static_cast<float>(FMath::Max(OutBreakdown.SpawnedVehicles, 1));
	OutBreakdown.ArrivalRate = OutBreakdown.ArrivedVehicles / Spawned;
	OutBreakdown.DeathRate = OutBreakdown.DeadVehicles / Spawned;
	OutBreakdown.TrafficOutcomeScore =
		180.0f * OutBreakdown.ArrivalRate
		+ 70.0f * FMath::Square(1.0f - OutBreakdown.DeathRate);

	OutBreakdown.TotalTravelTimeOfArrivedVehicles = TotalTravelTimeOfArrivedVehicles;
	OutBreakdown.TotalCellsTraversedByArrivedVehicles = TotalCellsTraversedByArrivedVehicles;
}

void UScoringManager::ComputeBudgetStats(FCityFlowScoreBreakdown& OutBreakdown) const
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const TArray<FGridVector> RoadCells = GM->GetCellsOfType(ECellType::Road);
	OutBreakdown.UsedBudget = RoadCells.Num();
	OutBreakdown.TotalRoadBudget = OutBreakdown.UsedBudget + GM->GetRemainingBudget();
	OutBreakdown.EstimatedMinRoadNeed = EstimateMinimumRoadNeed(GetAllBuildings());

	const float TotalRoadBudget = static_cast<float>(FMath::Max(OutBreakdown.TotalRoadBudget, 1));
	const float EstimatedMinRoadNeed = static_cast<float>(OutBreakdown.EstimatedMinRoadNeed);
	const float UsedBudget = static_cast<float>(OutBreakdown.UsedBudget);
	const float ConnectedRatio = OutBreakdown.TotalBuildingCount > 0
		? OutBreakdown.ConnectedBuildingCount / static_cast<float>(OutBreakdown.TotalBuildingCount)
		: 0.0f;

	const float BudgetWasteRatio = FMath::Clamp(
		(UsedBudget - EstimatedMinRoadNeed) / FMath::Max(TotalRoadBudget - EstimatedMinRoadNeed, 1.0f),
		0.0f,
		1.0f);
	const float BudgetEfficiencyRatio = 1.0f - BudgetWasteRatio;

	OutBreakdown.BudgetEfficiencyScore =
		150.0f
		* BudgetEfficiencyRatio
		* ConnectedRatio
		* FMath::Sqrt(OutBreakdown.ArrivalRate);
}

void UScoringManager::ComputeDifficultyMultiplier(FCityFlowScoreBreakdown& OutBreakdown) const
{
	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	const float ReferenceBuildingCount = Settings ? Settings->ReferenceBuildingCount : 8.0f;
	const float ReferenceSpreadRatio = Settings ? Settings->ReferenceSpreadRatio : 6.0f;
	const float TargetBudgetPressure = Settings ? Settings->TargetBudgetPressure : 0.45f;
	const float MinMultiplier = Settings ? Settings->MinMapDifficultyMultiplier : 0.85f;
	const float MaxMultiplier = Settings ? Settings->MaxMapDifficultyMultiplier : 1.20f;

	const float BuildingCountDifficulty =
		FMath::Clamp((OutBreakdown.TotalBuildingCount - ReferenceBuildingCount) / ReferenceBuildingCount, -0.25f, 0.35f) * 0.10f;

	const float SpreadRatio = OutBreakdown.EstimatedMinRoadNeed / static_cast<float>(FMath::Max(OutBreakdown.TotalBuildingCount, 1));
	const float SpreadDifficulty = ReferenceSpreadRatio > KINDA_SMALL_NUMBER
		? FMath::Clamp((SpreadRatio - ReferenceSpreadRatio) / ReferenceSpreadRatio, -0.5f, 0.8f) * 0.15f
		: 0.0f;

	const float BudgetPressure = OutBreakdown.EstimatedMinRoadNeed / static_cast<float>(FMath::Max(OutBreakdown.TotalRoadBudget, 1));
	const float BudgetPressureDifficulty = TargetBudgetPressure > KINDA_SMALL_NUMBER
		? FMath::Clamp((BudgetPressure - TargetBudgetPressure) / TargetBudgetPressure, -0.4f, 0.8f) * 0.15f
		: 0.0f;

	OutBreakdown.MapDifficultyMultiplier = FMath::Clamp(
		1.0f + BuildingCountDifficulty + SpreadDifficulty + BudgetPressureDifficulty,
		MinMultiplier,
		MaxMultiplier);
}

TArray<ABuilding*> UScoringManager::GetAllBuildings() const
{
	TArray<ABuilding*> Result;
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return Result;
	}

	const TArray<FGridVector> BuildingCells = GM->GetCellsOfType(ECellType::Building);
	TSet<int32> SeenIDs;
	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GM->GetCell(CellPos);
		if (Cell.BuildingID == INDEX_NONE || SeenIDs.Contains(Cell.BuildingID))
		{
			continue;
		}

		SeenIDs.Add(Cell.BuildingID);

		if (ABuilding* Building = Cast<ABuilding>(Cell.RoadActor))
		{
			Result.Add(Building);
		}
	}

	return Result;
}

bool UScoringManager::IsBuildingConnected(ABuilding* Building) const
{
	if (!Building)
	{
		return false;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	for (const FGridVector& DoorwayPos : Building->GetDoorwayWorldPositions())
	{
		if (GM->IsValidGridPos(DoorwayPos) && GM->GetCellType(DoorwayPos) == ECellType::Road)
		{
			return true;
		}
	}

	return false;
}

void UScoringManager::BuildRoadComponentMap(TMap<FGridVector, int32>& OutComponentByCell) const
{
	OutComponentByCell.Empty();

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	const TArray<FGridVector> RoadCells = GM->GetCellsOfType(ECellType::Road);
	TSet<FGridVector> Unvisited;
	for (const FGridVector& RoadCell : RoadCells)
	{
		Unvisited.Add(RoadCell);
	}
	int32 NextComponentId = 0;

	while (Unvisited.Num() > 0)
	{
		FGridVector StartCell;
		for (const FGridVector& Cell : Unvisited)
		{
			StartCell = Cell;
			break;
		}

		TArray<FGridVector> Queue;
		Queue.Add(StartCell);
		Unvisited.Remove(StartCell);

		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const FGridVector Current = Queue[QueueIndex];
			OutComponentByCell.Add(Current, NextComponentId);

			for (const EGridDirection Dir : GridDirectionUtils::GetAllDirections())
			{
				const FGridVector Neighbor = Current + GridDirectionUtils::GetVector(Dir);
				if (!Unvisited.Contains(Neighbor)
					|| !GM->IsValidGridPos(Neighbor)
					|| GM->GetCellType(Neighbor) != ECellType::Road)
				{
					continue;
				}

				Unvisited.Remove(Neighbor);
				Queue.Add(Neighbor);
			}
		}

		++NextComponentId;
	}
}

int32 UScoringManager::EstimateMinimumRoadNeed(const TArray<ABuilding*>& Buildings) const
{
	if (Buildings.Num() <= 1)
	{
		return 0;
	}

	TArray<FGridVector> Points;
	Points.Reserve(Buildings.Num());
	for (ABuilding* Building : Buildings)
	{
		if (Building)
		{
			Points.Add(Building->GetGridPosition());
		}
	}

	const int32 Count = Points.Num();
	if (Count <= 1)
	{
		return 0;
	}

	TArray<bool> bInTree;
	bInTree.Init(false, Count);

	TArray<int32> BestDistance;
	BestDistance.Init(MAX_int32, Count);
	BestDistance[0] = 0;

	int32 TotalLength = 0;
	for (int32 Step = 0; Step < Count; ++Step)
	{
		int32 BestIndex = INDEX_NONE;
		int32 BestValue = MAX_int32;
		for (int32 i = 0; i < Count; ++i)
		{
			if (!bInTree[i] && BestDistance[i] < BestValue)
			{
				BestValue = BestDistance[i];
				BestIndex = i;
			}
		}

		if (BestIndex == INDEX_NONE)
		{
			break;
		}

		bInTree[BestIndex] = true;
		TotalLength += BestValue;

		for (int32 i = 0; i < Count; ++i)
		{
			if (bInTree[i])
			{
				continue;
			}

			const int32 Dist = FMath::Abs(Points[BestIndex].X - Points[i].X)
				+ FMath::Abs(Points[BestIndex].Y - Points[i].Y);
			BestDistance[i] = FMath::Min(BestDistance[i], Dist);
		}
	}

	return TotalLength;
}

UVehicleManager* UScoringManager::GetVehicleManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UVehicleManager>();
}

UGridManager* UScoringManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}
