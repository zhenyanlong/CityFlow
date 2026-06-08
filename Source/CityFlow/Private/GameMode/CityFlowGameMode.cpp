#include "GameMode/CityFlowGameMode.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "GameMode/Types/BuildingDataAsset.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "Grid/Building.h"
#include "Grid/RoadTile.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Types/VehicleDataAsset.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowGM, Log, All);

ACityFlowGameMode::ACityFlowGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACityFlowGameMode::BeginPlay()
{
	Super::BeginPlay();

	RemainingBudget = TotalRoadBudget;
	PlayerBudget = FMath::RoundToInt(TotalRoadBudget * (1.0f - LSystemBudgetShare));
	LSystemBudget = TotalRoadBudget - PlayerBudget;

	if (UGridManager* GM = GetGridManager())
		GM->SetRoadBudget(TotalRoadBudget);
}

void ACityFlowGameMode::StartNewGame()
{
	if (CurrentPhase != ECityFlowGamePhase::None)
		return;

	InitializeDefaultScene();
	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::ReturnToMainMenu()
{
	// 停止一切
	GetWorldTimerManager().ClearTimer(SimulationTimerHandle);

	if (UVehicleManager* VM = GetVehicleManager())
	{
		VM->StopSpawning();
		VM->ClearAllVehicles();
	}

	if (UScoringManager* SM = GetScoringManager())
		SM->StopScoring();

	// 销毁所有已放置的 Actor
	for (TActorIterator<AGridPlaceableActor> It(GetWorld()); It; ++It)
		It->Destroy();

	// 重置网格
	if (UGridManager* GM = GetGridManager())
		GM->InitGrid(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, FVector::ZeroVector);

	// 重置 L-system
	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
		LSM->AbortGeneration();

	CurrentPhase = ECityFlowGamePhase::None;
}

void ACityFlowGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACityFlowGameMode::InitializeDefaultScene()
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	if (!GM->IsGridInitialized())
	{
		const FVector Origin = FVector(0.0f, 0.0f, 0.0f);
		GM->InitGrid(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, Origin);
	}

	TArray<FBuildingSpawnRequest> Requests;

	if (BuildingDataAsset)
	{
		// 使用 BuildingDataAsset 按权重占比确定各建筑生成数量
		const TArray<FBuildingDataEntry>& Entries = BuildingDataAsset->BuildingEntries;
		if (Entries.Num() > 0)
		{
			float TotalWeight = 0.0f;
			for (const FBuildingDataEntry& Entry : Entries)
			{
				TotalWeight += FMath::Max(0.0f, Entry.SpawnWeight);
			}

			if (TotalWeight > 0.0f)
			{
				// 按 weight / TotalWeight * DefaultBuildingCount 分配，取整后用最大余数法补足
				int32 AssignedTotal = 0;
				TArray<TPair<int32, float>> Fractionals; // (entry_index, fractional_part)

				for (int32 i = 0; i < Entries.Num(); ++i)
				{
					if (!Entries[i].BuildingClass) continue;

					const float Fraction = (Entries[i].SpawnWeight / TotalWeight) * DefaultBuildingCount;
					const int32 Count = FMath::FloorToInt(Fraction);
					AssignedTotal += Count;

					Fractionals.Add(TPair<int32, float>(i, Fraction - Count));

					if (Count > 0)
					{
						FBuildingSpawnRequest Req;
						Req.BuildingClass = Entries[i].BuildingClass;
						Req.Count = Count;
						Requests.Add(Req);
					}
				}

				// 余量按小数部分从大到小分配
				const int32 Remainder = DefaultBuildingCount - AssignedTotal;
				Fractionals.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B)
				{
					return A.Value > B.Value;
				});

				for (int32 i = 0; i < Remainder; ++i)
				{
					const int32 EntryIdx = Fractionals[i].Key;
					bool bFound = false;
					for (FBuildingSpawnRequest& Req : Requests)
					{
						if (Req.BuildingClass == Entries[EntryIdx].BuildingClass)
						{
							Req.Count++;
							bFound = true;
							break;
						}
					}
					if (!bFound)
					{
						FBuildingSpawnRequest Req;
						Req.BuildingClass = Entries[EntryIdx].BuildingClass;
						Req.Count = 1;
						Requests.Add(Req);
					}
				}
			}
		}
	}
	else
	{
		// 回退：使用旧的单类配置
		if (OriginBuildingClass || DestinationBuildingClass)
		{
			const int32 HalfCount = DefaultBuildingCount / 2;

			if (OriginBuildingClass)
			{
				FBuildingSpawnRequest OriginReq;
				OriginReq.BuildingClass = OriginBuildingClass;
				OriginReq.Count = HalfCount;
				Requests.Add(OriginReq);
			}

			if (DestinationBuildingClass)
			{
				FBuildingSpawnRequest DestReq;
				DestReq.BuildingClass = DestinationBuildingClass;
				DestReq.Count = DefaultBuildingCount - HalfCount;
				Requests.Add(DestReq);
			}
		}
	}

	if (Requests.Num() > 0)
	{
		GM->TryPlaceBuildingsRandom(Requests);
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (LSM && RoadTileClass)
	{
		LSM->SetRoadTileClass(RoadTileClass);
		LSM->SetBranchBudget(LSystemBudget);
	}
}

void ACityFlowGameMode::TransitionToPhase(ECityFlowGamePhase NewPhase)
{
	const ECityFlowGamePhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	OnGamePhaseChanged.Broadcast(OldPhase, NewPhase);

	switch (NewPhase)
	{
	case ECityFlowGamePhase::Planning:
		RemainingBudget = TotalRoadBudget;
		PlayerBudget = FMath::RoundToInt(TotalRoadBudget * (1.0f - LSystemBudgetShare));
		LSystemBudget = TotalRoadBudget - PlayerBudget;

		{
			ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
			if (LSM)
			{
				LSM->SetBranchBudget(LSystemBudget);
			}
		}
		break;

	case ECityFlowGamePhase::Simulating:
	{
		UVehicleManager* VM = GetVehicleManager();
		UScoringManager* SM = GetScoringManager();

		if (VM)
		{
			VM->SetDrivingSide(DrivingSide);
			VM->SetLaneOffsetFactor(LaneOffsetFactor);

			// 优先使用 GameMode 上的 VehicleDataAsset，否则 VehicleManager 回退到 DeveloperSettings
			if (VehicleDataAsset)
			{
				VM->SetVehicleDataAsset(VehicleDataAsset);
			}

			VM->StartSpawning();
		}

		if (SM)
		{
			SM->StartScoring();
		}

		SimulationTimeRemaining = SimulationDuration;
		GetWorldTimerManager().SetTimer(
			SimulationTimerHandle,
			this,
			&ACityFlowGameMode::OnSimulationTimerExpired,
			SimulationDuration,
			false
		);

		OnPlanningPhaseEnd.Broadcast();
		break;
	}

	case ECityFlowGamePhase::Evaluation:
	{
		UVehicleManager* VM = GetVehicleManager();
		UScoringManager* SM = GetScoringManager();

		if (VM)
		{
			VM->StopSpawning();
		}

		if (SM)
		{
			SM->StopScoring();
		}

		OnSimulationPhaseEnd.Broadcast();
		break;
	}

	default:
		break;
	}
}

void ACityFlowGameMode::StartSimulationPhase()
{
	if (CurrentPhase != ECityFlowGamePhase::Planning)
	{
		return;
	}

	TransitionToPhase(ECityFlowGamePhase::Simulating);
}

void ACityFlowGameMode::EndSimulationPhase()
{
	if (CurrentPhase != ECityFlowGamePhase::Simulating)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(SimulationTimerHandle);
	TransitionToPhase(ECityFlowGamePhase::Evaluation);
}

void ACityFlowGameMode::RestartPlanningPhase()
{
	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->ClearAllVehicles();
	}

	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::TriggerLSystemGrowth()
{
	if (CurrentPhase != ECityFlowGamePhase::Planning)
	{
		return;
	}

	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (!LSM)
	{
		return;
	}

	if (LSM->IsGenerating())
	{
		return;
	}

	LSM->SetBranchBudget(LSystemBudget);
	LSM->StartGenerate();
}

bool ACityFlowGameMode::CanPlaceRoad() const
{
	return CurrentPhase == ECityFlowGamePhase::Planning && PlayerBudget > 0;
}

bool ACityFlowGameMode::ConsumeRoadBudget(int32 Count)
{
	if (PlayerBudget < Count)
	{
		return false;
	}

	PlayerBudget -= Count;
	RemainingBudget -= Count;
	return true;
}

float ACityFlowGameMode::GetSimulationTimeRemaining() const
{
	if (CurrentPhase != ECityFlowGamePhase::Simulating)
	{
		return 0.0f;
	}

	if (GetWorldTimerManager().IsTimerActive(SimulationTimerHandle))
	{
		return GetWorldTimerManager().GetTimerRemaining(SimulationTimerHandle);
	}

	return 0.0f;
}

void ACityFlowGameMode::OnSimulationTimerExpired()
{
	EndSimulationPhase();
}

void ACityFlowGameMode::OnVehicleArrivedHandler(AVehicleActor* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}
}

bool ACityFlowGameMode::AreAllBuildingsConnected() const
{
	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (!LSM)
	{
		return false;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const TArray<FGridVector> BuildingCells = GM->GetCellsOfType(ECellType::Building);
	TSet<int32> SeenIDs;

	for (const FGridVector& CellPos : BuildingCells)
	{
		const FGridCell& Cell = GM->GetCell(CellPos);
		if (Cell.BuildingID != INDEX_NONE && !SeenIDs.Contains(Cell.BuildingID))
		{
			SeenIDs.Add(Cell.BuildingID);
		}
	}

	return true;
}

UGridManager* ACityFlowGameMode::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UGridManager>();
}

UVehicleManager* ACityFlowGameMode::GetVehicleManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UVehicleManager>();
}

UScoringManager* ACityFlowGameMode::GetScoringManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UScoringManager>();
}
