#include "GameMode/CityFlowGameMode.h"
#include "GameMode/Types/CityFlowGameTypes.h"
#include "GameMode/Types/BuildingDataAsset.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "Grid/GridPlaneVisualizer.h"
#include "Grid/GridVisualizer.h"
#include "Grid/Building.h"
#include "Grid/RoadTile.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Types/VehicleDataAsset.h"
#include "Scoring/Subsystem/ScoringManager.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "LSystem/Subsystem/LSystemManager.h"
#include "Environment/CityFlowLandscapeDecorationSettings.h"
#include "Environment/CityFlowRiverSettings.h"
#include "Environment/Subsystem/CityFlowLandscapeDecorationManager.h"
#include "Environment/Subsystem/CityFlowRiverManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowGM, Log, All);

ACityFlowGameMode::ACityFlowGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	EasyDifficultyProfile.BuildingCount = 8;
	EasyDifficultyProfile.VehicleSpawnInterval = 0.9f;
	EasyDifficultyProfile.SimulationDuration = 120.0f;
	EasyDifficultyProfile.RoadBudget = 220;
	EasyDifficultyProfile.TargetActiveVehicles = 16;
	EasyDifficultyProfile.SpawnBurstSize = 2;
	EasyDifficultyProfile.MaxActiveVehicles = 24;

	MediumDifficultyProfile.BuildingCount = 12;
	MediumDifficultyProfile.VehicleSpawnInterval = 0.65f;
	MediumDifficultyProfile.SimulationDuration = 180.0f;
	MediumDifficultyProfile.RoadBudget = 230;
	MediumDifficultyProfile.TargetActiveVehicles = 26;
	MediumDifficultyProfile.SpawnBurstSize = 3;
	MediumDifficultyProfile.MaxActiveVehicles = 36;

	HardDifficultyProfile.BuildingCount = 16;
	HardDifficultyProfile.VehicleSpawnInterval = 0.45f;
	HardDifficultyProfile.SimulationDuration = 240.0f;
	HardDifficultyProfile.RoadBudget = 240;
	HardDifficultyProfile.TargetActiveVehicles = 38;
	HardDifficultyProfile.SpawnBurstSize = 4;
	HardDifficultyProfile.MaxActiveVehicles = 50;
}

void ACityFlowGameMode::BeginPlay()
{
	Super::BeginPlay();
	ResetActiveMatchSettings();

	ActiveTotalRoadBudget = TotalRoadBudget;
	RemainingBudget = ActiveTotalRoadBudget;
	PlayerBudget = FMath::RoundToInt(ActiveTotalRoadBudget * (1.0f - LSystemBudgetShare));
	LSystemBudget = ActiveTotalRoadBudget - PlayerBudget;

	if (UGridManager* GM = GetGridManager())
		GM->SetRoadBudget(ActiveTotalRoadBudget);
}

void ACityFlowGameMode::StartNewGame()
{
	if (CurrentPhase != ECityFlowGamePhase::None && !bCurrentMatchIsMenuPreview)
		return;

	ResetRuntimeScene();
	ResetActiveMatchSettings();
	bCurrentMatchIsMenuPreview = false;
	bAutoStartSimulationAfterLSystem = false;
	InitializeScene(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, DefaultBuildingCount, TotalRoadBudget, INDEX_NONE);
	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::StartAutomatedRandomMatch(bool bAsMenuPreview)
{
	ResetRuntimeScene();
	ResetActiveMatchSettings();

	bCurrentMatchIsMenuPreview = bAsMenuPreview;
	bAutoStartSimulationAfterLSystem = true;

	int32 MatchGridWidth = DefaultGridWidth;
	int32 MatchGridHeight = DefaultGridHeight;
	int32 MatchBuildingCount = DefaultBuildingCount;
	int32 MatchRoadBudget = TotalRoadBudget;
	int32 RandomSeed = INDEX_NONE;
	PickRandomSceneParameters(MatchGridWidth, MatchGridHeight, MatchBuildingCount, MatchRoadBudget, RandomSeed);

	InitializeScene(
		FMath::Max(4, MatchGridWidth),
		FMath::Max(4, MatchGridHeight),
		DefaultCellSize,
		FMath::Max(2, MatchBuildingCount),
		FMath::Max(1, MatchRoadBudget),
		RandomSeed);

	TransitionToPhase(ECityFlowGamePhase::Planning);
	StartAutoRoadGenerationOrSimulation();
}

void ACityFlowGameMode::StartRandomPlanningGame()
{
	StartRandomPlanningGameWithDifficulty(ECityFlowDifficulty::Medium);
}

void ACityFlowGameMode::StartRandomPlanningGameWithDifficulty(ECityFlowDifficulty Difficulty)
{
	ResetRuntimeScene();
	ApplyDifficultyProfile(Difficulty);

	bCurrentMatchIsMenuPreview = false;
	bAutoStartSimulationAfterLSystem = false;

	int32 MatchGridWidth = DefaultGridWidth;
	int32 MatchGridHeight = DefaultGridHeight;
	int32 MatchBuildingCount = DefaultBuildingCount;
	int32 MatchRoadBudget = TotalRoadBudget;
	int32 RandomSeed = INDEX_NONE;
	PickRandomSceneParameters(MatchGridWidth, MatchGridHeight, MatchBuildingCount, MatchRoadBudget, RandomSeed);

	const FCityFlowDifficultyProfile Profile = GetDifficultyProfile(Difficulty);
	MatchBuildingCount = Profile.BuildingCount;
	MatchRoadBudget = Profile.RoadBudget;

	InitializeScene(
		FMath::Max(4, MatchGridWidth),
		FMath::Max(4, MatchGridHeight),
		DefaultCellSize,
		FMath::Max(2, MatchBuildingCount),
		FMath::Max(1, MatchRoadBudget),
		RandomSeed);

	TransitionToPhase(ECityFlowGamePhase::Planning);
}

void ACityFlowGameMode::ReturnToMainMenu()
{
	ResetRuntimeScene();
}

void ACityFlowGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACityFlowGameMode::InitializeDefaultScene()
{
	InitializeScene(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, DefaultBuildingCount, TotalRoadBudget, INDEX_NONE);
}

void ACityFlowGameMode::InitializeScene(int32 GridWidth, int32 GridHeight, float CellSize, int32 BuildingCount, int32 RoadBudget, int32 RandomSeed)
{
	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return;
	}

	ActiveTotalRoadBudget = FMath::Max(0, RoadBudget);
	RemainingBudget = ActiveTotalRoadBudget;
	PlayerBudget = FMath::RoundToInt(ActiveTotalRoadBudget * (1.0f - LSystemBudgetShare));
	LSystemBudget = ActiveTotalRoadBudget - PlayerBudget;

	const FVector Origin = FVector(0.0f, 0.0f, 0.0f);
	GM->InitGrid(GridWidth, GridHeight, CellSize, Origin);
	GM->SetRoadBudget(ActiveTotalRoadBudget);
	RefreshGridVisualizers();

	if (const UCityFlowRiverSettings* RiverSettings = GetDefault<UCityFlowRiverSettings>())
	{
		if (RiverSettings->bAutoGenerateOnNewGame)
		{
			if (UCityFlowRiverManager* RiverManager = GetWorld()->GetSubsystem<UCityFlowRiverManager>())
			{
				const int32 RiverSeed = RandomSeed == INDEX_NONE ? RiverSettings->RandomSeed : RandomSeed;
				RiverManager->GenerateRivers(RiverSeed);
			}
		}
	}

	if (const UCityFlowLandscapeDecorationSettings* LandscapeSettings = GetDefault<UCityFlowLandscapeDecorationSettings>())
	{
		if (LandscapeSettings->bAutoGenerateOnNewGame)
		{
			if (UCityFlowLandscapeDecorationManager* LandscapeManager = GetWorld()->GetSubsystem<UCityFlowLandscapeDecorationManager>())
			{
				const int32 DecorationSeed = RandomSeed == INDEX_NONE ? LandscapeSettings->RandomSeed : RandomSeed + 1;
				LandscapeManager->GenerateDecorations(DecorationSeed);
			}
		}
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

					const float Fraction = (Entries[i].SpawnWeight / TotalWeight) * BuildingCount;
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
				const int32 Remainder = BuildingCount - AssignedTotal;
				Fractionals.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B)
				{
					return A.Value > B.Value;
				});

				for (int32 i = 0; i < Remainder && Fractionals.IsValidIndex(i); ++i)
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
			const int32 HalfCount = BuildingCount / 2;

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
				DestReq.Count = BuildingCount - HalfCount;
				Requests.Add(DestReq);
			}
		}
	}

	if (Requests.Num() > 0)
	{
		GM->TryPlaceBuildingsRandom(Requests);
	}

	ConfigureLSystemForActiveScene();
}

void ACityFlowGameMode::TransitionToPhase(ECityFlowGamePhase NewPhase)
{
	const ECityFlowGamePhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;

	OnGamePhaseChanged.Broadcast(OldPhase, NewPhase);

	switch (NewPhase)
	{
	case ECityFlowGamePhase::Planning:
		RemainingBudget = GetActiveTotalRoadBudget();
		PlayerBudget = FMath::RoundToInt(GetActiveTotalRoadBudget() * (1.0f - LSystemBudgetShare));
		LSystemBudget = GetActiveTotalRoadBudget() - PlayerBudget;

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
			VM->ConfigureSpawnProfile(
				ActiveVehicleSpawnInterval,
				ActiveVehicleTargetCount,
				ActiveMaxVehicleCount,
				ActiveVehicleSpawnBurstSize);

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

		SimulationTimeRemaining = ActiveSimulationDuration;
		GetWorldTimerManager().SetTimer(
			SimulationTimerHandle,
			this,
			&ACityFlowGameMode::OnSimulationTimerExpired,
			ActiveSimulationDuration,
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
	GetWorldTimerManager().ClearTimer(SimulationTimerHandle);

	UVehicleManager* VM = GetVehicleManager();
	if (VM)
	{
		VM->ClearAllVehicles();
	}

	if (UScoringManager* SM = GetScoringManager())
	{
		SM->StopScoring();
	}

	bCurrentMatchIsMenuPreview = false;
	bAutoStartSimulationAfterLSystem = false;
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

	// Automated title matches have no player planning pass, so the generator may
	// use the whole remaining pool. Player-triggered growth keeps its configured share.
	const UGridManager* GM = GetGridManager();
	const int32 GenerationBudget = bAutoStartSimulationAfterLSystem && GM
		? GM->GetRemainingBudget()
		: LSystemBudget;
	LSM->SetBranchBudget(GenerationBudget);
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

void ACityFlowGameMode::HandleAutoLSystemFinished(bool bAllConnected)
{
	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationFinished.RemoveDynamic(this, &ACityFlowGameMode::HandleAutoLSystemFinished);
	}

	if (!bAutoStartSimulationAfterLSystem)
	{
		return;
	}

	bAutoStartSimulationAfterLSystem = false;

	if (CurrentPhase == ECityFlowGamePhase::Planning)
	{
		StartSimulationPhase();
	}
}

FCityFlowDifficultyProfile ACityFlowGameMode::GetDifficultyProfile(ECityFlowDifficulty Difficulty) const
{
	switch (Difficulty)
	{
	case ECityFlowDifficulty::Easy:
		return EasyDifficultyProfile;
	case ECityFlowDifficulty::Hard:
		return HardDifficultyProfile;
	case ECityFlowDifficulty::Medium:
	default:
		return MediumDifficultyProfile;
	}
}

void ACityFlowGameMode::ResetActiveMatchSettings()
{
	ActiveDifficulty = ECityFlowDifficulty::Medium;
	ActiveSimulationDuration = FMath::Max(1.0f, SimulationDuration);

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	ActiveVehicleSpawnInterval = Settings ? FMath::Max(0.1f, Settings->VehicleSpawnInterval) : 0.65f;
	ActiveVehicleTargetCount = Settings ? FMath::Max(1, Settings->TargetActiveVehicleCount) : 24;
	ActiveVehicleSpawnBurstSize = Settings ? FMath::Max(1, Settings->MaxSpawnBurstSize) : 3;
	ActiveMaxVehicleCount = Settings
		? FMath::Max(ActiveVehicleTargetCount, Settings->MaxVehicleCount)
		: 40;
}

void ACityFlowGameMode::ApplyDifficultyProfile(ECityFlowDifficulty Difficulty)
{
	const FCityFlowDifficultyProfile Profile = GetDifficultyProfile(Difficulty);
	ActiveDifficulty = Difficulty;
	ActiveSimulationDuration = FMath::Max(1.0f, Profile.SimulationDuration);
	ActiveVehicleSpawnInterval = FMath::Max(0.1f, Profile.VehicleSpawnInterval);
	ActiveVehicleTargetCount = FMath::Max(1, Profile.TargetActiveVehicles);
	ActiveVehicleSpawnBurstSize = FMath::Max(1, Profile.SpawnBurstSize);
	ActiveMaxVehicleCount = FMath::Max(ActiveVehicleTargetCount, Profile.MaxActiveVehicles);
}

void ACityFlowGameMode::ResetRuntimeScene()
{
	bAutoStartSimulationAfterLSystem = false;

	GetWorldTimerManager().ClearTimer(SimulationTimerHandle);
	SimulationTimeRemaining = 0.0f;

	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		LSM->OnGenerationFinished.RemoveDynamic(this, &ACityFlowGameMode::HandleAutoLSystemFinished);
		LSM->AbortGeneration();
	}

	if (UVehicleManager* VM = GetVehicleManager())
	{
		VM->ClearAllVehicles();
	}

	if (UScoringManager* SM = GetScoringManager())
	{
		SM->StopScoring();
	}

	if (UCityFlowLandscapeDecorationManager* LandscapeManager = GetWorld()->GetSubsystem<UCityFlowLandscapeDecorationManager>())
	{
		LandscapeManager->ClearDecorations();
	}

	if (UCityFlowRiverManager* RiverManager = GetWorld()->GetSubsystem<UCityFlowRiverManager>())
	{
		RiverManager->ClearRivers();
	}

	for (TActorIterator<AGridPlaceableActor> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}

	if (UGridManager* GM = GetGridManager())
	{
		GM->InitGrid(DefaultGridWidth, DefaultGridHeight, DefaultCellSize, FVector::ZeroVector);
		GM->SetRoadBudget(TotalRoadBudget);
	}

	ActiveTotalRoadBudget = TotalRoadBudget;
	RemainingBudget = TotalRoadBudget;
	PlayerBudget = FMath::RoundToInt(TotalRoadBudget * (1.0f - LSystemBudgetShare));
	LSystemBudget = TotalRoadBudget - PlayerBudget;
	CurrentPhase = ECityFlowGamePhase::None;
	bCurrentMatchIsMenuPreview = false;
}

void ACityFlowGameMode::PickRandomSceneParameters(int32& OutGridWidth, int32& OutGridHeight, int32& OutBuildingCount, int32& OutRoadBudget, int32& OutRandomSeed) const
{
	OutRandomSeed = static_cast<int32>(FPlatformTime::Cycles() & 0x7fffffff);
	FMath::RandInit(OutRandomSeed);

	auto PickIntInRange = [](const FIntPoint& Range, int32 Fallback)
	{
		const int32 MinValue = FMath::Min(Range.X, Range.Y);
		const int32 MaxValue = FMath::Max(Range.X, Range.Y);
		return MaxValue >= MinValue ? FMath::RandRange(MinValue, MaxValue) : Fallback;
	};

	OutGridWidth = bRandomizeAutoMatchParameters
		? PickIntInRange(AutoMatchGridWidthRange, DefaultGridWidth)
		: DefaultGridWidth;
	OutGridHeight = bRandomizeAutoMatchParameters
		? PickIntInRange(AutoMatchGridHeightRange, DefaultGridHeight)
		: DefaultGridHeight;
	OutBuildingCount = bRandomizeAutoMatchParameters
		? PickIntInRange(AutoMatchBuildingCountRange, DefaultBuildingCount)
		: DefaultBuildingCount;
	OutRoadBudget = bRandomizeAutoMatchParameters
		? PickIntInRange(AutoMatchRoadBudgetRange, TotalRoadBudget)
		: TotalRoadBudget;
}

void ACityFlowGameMode::RefreshGridVisualizers()
{
	for (TActorIterator<AGridVisualizer> It(GetWorld()); It; ++It)
	{
		It->RedrawGrid();
	}

	for (TActorIterator<AGridPlaneVisualizer> It(GetWorld()); It; ++It)
	{
		It->SetupPlane();
	}
}

void ACityFlowGameMode::ConfigureLSystemForActiveScene()
{
	if (ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>())
	{
		if (RoadTileClass)
		{
			LSM->SetRoadTileClass(RoadTileClass);
		}
		LSM->SetBranchBudget(LSystemBudget);
	}
}

void ACityFlowGameMode::StartAutoRoadGenerationOrSimulation()
{
	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	if (!LSM || !RoadTileClass)
	{
		HandleAutoLSystemFinished(false);
		return;
	}

	LSM->OnGenerationFinished.RemoveDynamic(this, &ACityFlowGameMode::HandleAutoLSystemFinished);
	LSM->OnGenerationFinished.AddDynamic(this, &ACityFlowGameMode::HandleAutoLSystemFinished);
	TriggerLSystemGrowth();

	if (!LSM->IsGenerating() && bAutoStartSimulationAfterLSystem)
	{
		HandleAutoLSystemFinished(AreAllBuildingsConnected());
	}
}

int32 ACityFlowGameMode::GetActiveTotalRoadBudget() const
{
	return ActiveTotalRoadBudget > 0 ? ActiveTotalRoadBudget : TotalRoadBudget;
}

bool ACityFlowGameMode::AreAllBuildingsConnected() const
{
	ULSystemManager* LSM = GetWorld()->GetSubsystem<ULSystemManager>();
	return LSM && LSM->AreAllBuildingsConnected();
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
