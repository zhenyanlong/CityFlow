#include "Environment/Subsystem/CityFlowLandscapeDecorationManager.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Engine/World.h"
#include "Environment/CityFlowLandscapeDecorationSettings.h"
#include "Environment/Subsystem/CityFlowRiverManager.h"
#include "GameFramework/Actor.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogCityFlowLandscapeDecoration, Log, All);

void UCityFlowLandscapeDecorationManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UCityFlowLandscapeDecorationManager::Deinitialize()
{
	UnbindGridEvents();
	ClearDecorations();
	Super::Deinitialize();
}

void UCityFlowLandscapeDecorationManager::GenerateDecorations(int32 Seed)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const UCityFlowLandscapeDecorationSettings* Settings = GetDefault<UCityFlowLandscapeDecorationSettings>();
	if (!Settings || (Settings->Decorations.Num() == 0 && !Settings->GrassCoverage.bEnabled))
	{
		return;
	}

	BindGridEvents();
	ClearDecorations();

	if (!EnsureRootActor())
	{
		return;
	}

	const int32 EffectiveSeed = (Seed == INDEX_NONE) ? Settings->RandomSeed : Seed;
	FRandomStream RandomStream(EffectiveSeed);
	const int32 AttemptsPerCell = FMath::Max(0, Settings->InstancesPerCell);

	GenerateGrassCoverage(Settings->GrassCoverage, RandomStream);

	for (int32 Y = 0; Y < GM->GetGridHeight(); ++Y)
	{
		for (int32 X = 0; X < GM->GetGridWidth(); ++X)
		{
			const FGridVector CellPos(X, Y);
			if (GM->GetCellType(CellPos) != ECellType::Empty)
			{
				continue;
			}

			if (const UCityFlowRiverManager* RiverManager = GetWorld()->GetSubsystem<UCityFlowRiverManager>())
			{
				if (RiverManager->IsRiverOrBankCell(CellPos))
				{
					continue;
				}
			}

			for (int32 Attempt = 0; Attempt < AttemptsPerCell; ++Attempt)
			{
				TrySpawnDecorationInCell(CellPos, Settings->Decorations, RandomStream);
			}
		}
	}
}

void UCityFlowLandscapeDecorationManager::ClearDecorations()
{
	if (RootActor.IsValid())
	{
		RootActor->Destroy();
	}

	RootActor.Reset();
	ComponentsByConfigIndex.Empty();
	InstanceRecords.Empty();
	CellToInstanceIds.Empty();
	GrassCoverageComponent.Reset();
	NextInstanceId = 1;
	LiveInstanceCount = 0;
}

void UCityFlowLandscapeDecorationManager::ClearDecorationsForCell(FGridVector CellPos, int32 ExtraPaddingCells)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	TSet<int32> InstanceIds;
	const int32 Padding = FMath::Max(0, ExtraPaddingCells);

	for (int32 Y = CellPos.Y - Padding; Y <= CellPos.Y + Padding; ++Y)
	{
		for (int32 X = CellPos.X - Padding; X <= CellPos.X + Padding; ++X)
		{
			const FGridVector QueryCell(X, Y);
			if (!GM->IsValidGridPos(QueryCell))
			{
				continue;
			}

			if (const TSet<int32>* CellInstanceIds = CellToInstanceIds.Find(QueryCell))
			{
				for (int32 InstanceId : *CellInstanceIds)
				{
					InstanceIds.Add(InstanceId);
				}
			}
		}
	}

	ClearDecorationIds(InstanceIds);
}

void UCityFlowLandscapeDecorationManager::ClearDecorationsForCells(const TArray<FGridVector>& Cells, int32 ExtraPaddingCells)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	TSet<int32> InstanceIds;
	const int32 Padding = FMath::Max(0, ExtraPaddingCells);

	for (const FGridVector& CellPos : Cells)
	{
		for (int32 Y = CellPos.Y - Padding; Y <= CellPos.Y + Padding; ++Y)
		{
			for (int32 X = CellPos.X - Padding; X <= CellPos.X + Padding; ++X)
			{
				const FGridVector QueryCell(X, Y);
				if (!GM->IsValidGridPos(QueryCell))
				{
					continue;
				}

				if (const TSet<int32>* CellInstanceIds = CellToInstanceIds.Find(QueryCell))
				{
					for (int32 InstanceId : *CellInstanceIds)
					{
						InstanceIds.Add(InstanceId);
					}
				}
			}
		}
	}

	ClearDecorationIds(InstanceIds);
}

void UCityFlowLandscapeDecorationManager::HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell)
{
	if (NewCell.Type != ECellType::Road && NewCell.Type != ECellType::Building)
	{
		return;
	}

	const UCityFlowLandscapeDecorationSettings* Settings = GetDefault<UCityFlowLandscapeDecorationSettings>();
	const int32 Padding = Settings ? Settings->PlacementClearPaddingCells : 0;
	ClearDecorationsForCell(CellPos, Padding);
}

void UCityFlowLandscapeDecorationManager::HandleGridPlaced(AGridPlaceableActor* PlacedActor)
{
	if (!PlacedActor)
	{
		return;
	}

	const UCityFlowLandscapeDecorationSettings* Settings = GetDefault<UCityFlowLandscapeDecorationSettings>();
	const int32 Padding = Settings ? Settings->PlacementClearPaddingCells : 0;
	ClearDecorationsForCells(PlacedActor->GetOccupiedCells(), Padding);
}

void UCityFlowLandscapeDecorationManager::BindGridEvents()
{
	if (bGridEventsBound)
	{
		return;
	}

	if (UGridManager* GM = GetGridManager())
	{
		GM->OnCellChanged.AddDynamic(this, &UCityFlowLandscapeDecorationManager::HandleCellChanged);
		GM->OnGridPlaced.AddDynamic(this, &UCityFlowLandscapeDecorationManager::HandleGridPlaced);
		bGridEventsBound = true;
	}
}

void UCityFlowLandscapeDecorationManager::UnbindGridEvents()
{
	if (!bGridEventsBound)
	{
		return;
	}

	if (UGridManager* GM = GetGridManager())
	{
		GM->OnCellChanged.RemoveDynamic(this, &UCityFlowLandscapeDecorationManager::HandleCellChanged);
		GM->OnGridPlaced.RemoveDynamic(this, &UCityFlowLandscapeDecorationManager::HandleGridPlaced);
	}

	bGridEventsBound = false;
}

bool UCityFlowLandscapeDecorationManager::EnsureRootActor()
{
	if (RootActor.IsValid())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = TEXT("CityFlowLandscapeDecorations");

	AActor* NewRootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewRootActor)
	{
		return false;
	}

	USceneComponent* SceneRoot = NewObject<USceneComponent>(NewRootActor, TEXT("SceneRoot"));
	NewRootActor->AddInstanceComponent(SceneRoot);
	NewRootActor->SetRootComponent(SceneRoot);
	SceneRoot->RegisterComponent();

	RootActor = NewRootActor;
	return true;
}

UHierarchicalInstancedStaticMeshComponent* UCityFlowLandscapeDecorationManager::GetOrCreateComponent(
	int32 ConfigIndex,
	const FCityFlowLandscapeDecorationConfig& Config)
{
	if (ConfigIndex < 0)
	{
		return nullptr;
	}

	ComponentsByConfigIndex.SetNum(FMath::Max(ComponentsByConfigIndex.Num(), ConfigIndex + 1));
	if (ComponentsByConfigIndex[ConfigIndex].IsValid())
	{
		return ComponentsByConfigIndex[ConfigIndex].Get();
	}

	if (!EnsureRootActor())
	{
		return nullptr;
	}

	UStaticMesh* Mesh = Config.Mesh.LoadSynchronous();
	if (!Mesh)
	{
		return nullptr;
	}

	AActor* Owner = RootActor.Get();
	const FString SafeName = Config.Name.IsNone()
		? FString::Printf(TEXT("Scenery_%d"), ConfigIndex)
		: Config.Name.ToString();

	UHierarchicalInstancedStaticMeshComponent* Component =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(
			Owner,
			*FString::Printf(TEXT("HISM_%s"), *SafeName));

	Owner->AddInstanceComponent(Component);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(true);

	if (UMaterialInterface* Material = Config.OverrideMaterial.LoadSynchronous())
	{
		Component->SetMaterial(0, Material);
	}

	if (USceneComponent* RootComponent = Owner->GetRootComponent())
	{
		Component->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}

	Component->RegisterComponent();
	ComponentsByConfigIndex[ConfigIndex] = Component;
	return Component;
}

UHierarchicalInstancedStaticMeshComponent* UCityFlowLandscapeDecorationManager::GetOrCreateGrassCoverageComponent(
	const FCityFlowGrassCoverageConfig& Config)
{
	if (GrassCoverageComponent.IsValid())
	{
		return GrassCoverageComponent.Get();
	}

	if (!EnsureRootActor())
	{
		return nullptr;
	}

	UStaticMesh* Mesh = Config.GrassMesh.LoadSynchronous();
	if (!Mesh)
	{
		return nullptr;
	}

	AActor* Owner = RootActor.Get();
	UHierarchicalInstancedStaticMeshComponent* Component =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner, TEXT("HISM_GrassCoverage"));

	Owner->AddInstanceComponent(Component);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);

	if (UMaterialInterface* Material = Config.OverrideMaterial.LoadSynchronous())
	{
		Component->SetMaterial(0, Material);
	}

	if (USceneComponent* RootComponent = Owner->GetRootComponent())
	{
		Component->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}

	Component->RegisterComponent();
	GrassCoverageComponent = Component;
	return Component;
}

void UCityFlowLandscapeDecorationManager::GenerateGrassCoverage(
	const FCityFlowGrassCoverageConfig& Config,
	FRandomStream& RandomStream)
{
	if (!Config.bEnabled || Config.DensityPerCell <= 0)
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Log,
			TEXT("Grass coverage skipped: Enabled=%s DensityPerCell=%d"),
			Config.bEnabled ? TEXT("true") : TEXT("false"),
			Config.DensityPerCell);
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Warning, TEXT("Grass coverage skipped: GridManager is not initialized."));
		return;
	}

	UStaticMesh* GrassMesh = Config.GrassMesh.LoadSynchronous();
	UTexture2D* GroundColorTexture = Config.GroundColorTexture.LoadSynchronous();
	if (!GrassMesh || !GroundColorTexture)
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Warning,
			TEXT("Grass coverage skipped: GrassMesh=%s GroundColorTexture=%s"),
			GrassMesh ? *GrassMesh->GetName() : TEXT("None"),
			GroundColorTexture ? *GroundColorTexture->GetName() : TEXT("None"));
		return;
	}

	TArray<FColor> GroundColorPixels;
	int32 TextureWidth = 0;
	int32 TextureHeight = 0;
	if (!ReadGroundColorPixels(GroundColorTexture, GroundColorPixels, TextureWidth, TextureHeight))
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Warning,
			TEXT("Grass coverage skipped: failed to read pixels from %s. Use an uncompressed/readable color texture or keep source data available."),
			*GroundColorTexture->GetName());
		return;
	}

	if (!GetOrCreateGrassCoverageComponent(Config))
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Warning, TEXT("Grass coverage skipped: failed to create grass HISM component."));
		return;
	}

	int32 EligibleCells = 0;
	int32 SampleAttempts = 0;
	int32 GreenSamples = 0;
	int32 SpawnedInstances = 0;
	int32 BelowMinSamples = 0;
	int32 TransitionSamples = 0;
	int32 FullSamples = 0;
	int32 ValidRatioSamples = 0;
	double RatioSum = 0.0;
	float MinObservedRatio = TNumericLimits<float>::Max();
	float MaxObservedRatio = TNumericLimits<float>::Lowest();

	for (int32 Y = 0; Y < GM->GetGridHeight(); ++Y)
	{
		for (int32 X = 0; X < GM->GetGridWidth(); ++X)
		{
			const FGridVector CellPos(X, Y);

			if (Config.bRejectOccupiedCells && GM->GetCellType(CellPos) != ECellType::Empty)
			{
				continue;
			}

			if (const UCityFlowRiverManager* RiverManager = GetWorld()->GetSubsystem<UCityFlowRiverManager>())
			{
				if (RiverManager->IsRiverOrBankCell(CellPos))
				{
					continue;
				}
			}

			++EligibleCells;
			TrySpawnGrassInCell(
				CellPos,
				Config,
				GrassMesh,
				GroundColorPixels,
				TextureWidth,
				TextureHeight,
				RandomStream,
				SampleAttempts,
				GreenSamples,
				SpawnedInstances,
				BelowMinSamples,
				TransitionSamples,
				FullSamples,
				ValidRatioSamples,
				RatioSum,
				MinObservedRatio,
				MaxObservedRatio);
		}
	}

	const float AverageObservedRatio = ValidRatioSamples > 0
		? static_cast<float>(RatioSum / static_cast<double>(ValidRatioSamples))
		: 0.0f;
	const float SafeMinObservedRatio = ValidRatioSamples > 0 ? MinObservedRatio : 0.0f;
	const float SafeMaxObservedRatio = ValidRatioSamples > 0 ? MaxObservedRatio : 0.0f;
	const float EffectiveFullRatio = FMath::Max(Config.GreenRatioPivot, Config.GreenRatioMin + 0.05f);

	UE_LOG(LogCityFlowLandscapeDecoration, Log,
		TEXT("Grass coverage generated: Texture=%s Size=%dx%d EligibleCells=%d Samples=%d ValidRatio=%d BelowMin=%d Transition=%d Full=%d GreenSamples=%d Spawned=%d RatioObserved=(%.3f, %.3f, %.3f) GreenRatioMin=%.3f GreenRatioFull=%.3f DryRatio=%.3f Tile=(%.6f, %.6f) Offset=(%.3f, %.3f)"),
		*GroundColorTexture->GetName(),
		TextureWidth,
		TextureHeight,
		EligibleCells,
		SampleAttempts,
		ValidRatioSamples,
		BelowMinSamples,
		TransitionSamples,
		FullSamples,
		GreenSamples,
		SpawnedInstances,
		SafeMinObservedRatio,
		AverageObservedRatio,
		SafeMaxObservedRatio,
		Config.GreenRatioMin,
		EffectiveFullRatio,
		Config.DryGrassRatio,
		Config.MaterialTile.X,
		Config.MaterialTile.Y,
		Config.MaterialOffset.X,
		Config.MaterialOffset.Y);
}

bool UCityFlowLandscapeDecorationManager::ReadGroundColorPixels(
	UTexture2D* GroundColorTexture,
	TArray<FColor>& OutPixels,
	int32& OutWidth,
	int32& OutHeight) const
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	if (!GroundColorTexture)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (GroundColorTexture->Source.IsValid())
	{
		const ETextureSourceFormat SourceFormat = GroundColorTexture->Source.GetFormat();
		OutWidth = GroundColorTexture->Source.GetSizeX();
		OutHeight = GroundColorTexture->Source.GetSizeY();

		TArray64<uint8> SourceData;
		if (OutWidth > 0 && OutHeight > 0 && GroundColorTexture->Source.GetMipData(SourceData, 0))
		{
			OutPixels.SetNum(OutWidth * OutHeight);

			if (SourceFormat == TSF_BGRA8 || SourceFormat == TSF_BGRE8)
			{
				const FColor* SourceColors = reinterpret_cast<const FColor*>(SourceData.GetData());
				for (int32 i = 0; i < OutPixels.Num(); ++i)
				{
					OutPixels[i] = SourceColors[i];
				}
				return true;
			}

			if (SourceFormat == TSF_G8)
			{
				for (int32 i = 0; i < OutPixels.Num(); ++i)
				{
					const uint8 G = SourceData[i];
					OutPixels[i] = FColor(G, G, G, 255);
				}
				return true;
			}

			UE_LOG(LogCityFlowLandscapeDecoration, Warning,
				TEXT("Unsupported texture source format %d for grass sampling on %s."),
				static_cast<int32>(SourceFormat),
				*GroundColorTexture->GetName());
		}
	}
#endif

	if (!GroundColorTexture->GetPlatformData() || GroundColorTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return false;
	}

	FTexture2DMipMap& Mip = GroundColorTexture->GetPlatformData()->Mips[0];
	OutWidth = Mip.SizeX;
	OutHeight = Mip.SizeY;
	if (OutWidth <= 0 || OutHeight <= 0)
	{
		return false;
	}

	if (GroundColorTexture->GetPlatformData()->PixelFormat != PF_B8G8R8A8)
	{
		UE_LOG(LogCityFlowLandscapeDecoration, Warning,
			TEXT("Unsupported platform pixel format %d for grass sampling on %s; expected PF_B8G8R8A8."),
			static_cast<int32>(GroundColorTexture->GetPlatformData()->PixelFormat),
			*GroundColorTexture->GetName());
		return false;
	}

	const FColor* SourcePixels = static_cast<const FColor*>(Mip.BulkData.LockReadOnly());
	if (!SourcePixels)
	{
		Mip.BulkData.Unlock();
		return false;
	}

	OutPixels.Append(SourcePixels, OutWidth * OutHeight);
	Mip.BulkData.Unlock();
	return OutPixels.Num() > 0;
}

bool UCityFlowLandscapeDecorationManager::TrySpawnGrassInCell(
	const FGridVector& CellPos,
	const FCityFlowGrassCoverageConfig& Config,
	UStaticMesh* GrassMesh,
	const TArray<FColor>& GroundColorPixels,
	int32 TextureWidth,
	int32 TextureHeight,
	FRandomStream& RandomStream,
	int32& OutSampleAttempts,
	int32& OutGreenSamples,
	int32& OutSpawnedInstances,
	int32& OutBelowMinSamples,
	int32& OutTransitionSamples,
	int32& OutFullSamples,
	int32& OutValidRatioSamples,
	double& OutRatioSum,
	float& OutMinObservedRatio,
	float& OutMaxObservedRatio)
{
	UGridManager* GM = GetGridManager();
	UHierarchicalInstancedStaticMeshComponent* Component = GrassCoverageComponent.Get();
	if (!GM || !Component || !GrassMesh || GroundColorPixels.Num() == 0 || TextureWidth <= 0 || TextureHeight <= 0)
	{
		return false;
	}

	bool bSpawnedAny = false;
	const float CellSize = GM->GetCellSize();
	const float OffsetRange = FMath::Clamp(Config.CellRandomOffset, 0.0f, 0.49f) * CellSize;
	const int32 Attempts = FMath::Max(0, Config.DensityPerCell);

	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		++OutSampleAttempts;

		FVector Location = GM->GridToWorld(CellPos);
		Location.X += RandomStream.FRandRange(-OffsetRange, OffsetRange);
		Location.Y += RandomStream.FRandRange(-OffsetRange, OffsetRange);
		Location.Z += Config.ZOffset;

		float GrassScore = 0.0f;
		float GreenRatio = 0.0f;
		if (!SampleGroundGrassScore(GroundColorPixels, TextureWidth, TextureHeight, Location, Config, GrassScore, GreenRatio))
		{
			continue;
		}

		++OutValidRatioSamples;
		OutRatioSum += GreenRatio;
		OutMinObservedRatio = FMath::Min(OutMinObservedRatio, GreenRatio);
		OutMaxObservedRatio = FMath::Max(OutMaxObservedRatio, GreenRatio);

		const float EffectiveFullRatio = FMath::Max(Config.GreenRatioPivot, Config.GreenRatioMin + 0.05f);
		if (GreenRatio < Config.GreenRatioMin)
		{
			++OutBelowMinSamples;
		}
		else if (GreenRatio >= EffectiveFullRatio)
		{
			++OutFullSamples;
		}
		else
		{
			++OutTransitionSamples;
		}

		if (GrassScore <= 0.0f)
		{
			continue;
		}

		++OutGreenSamples;

		const float SpawnChance = FMath::Clamp(GrassScore, 0.0f, 1.0f);
		if (RandomStream.FRand() > SpawnChance)
		{
			continue;
		}

		const float ScaleMin = FMath::Min(Config.UniformScaleRange.X, Config.UniformScaleRange.Y);
		const float ScaleMax = FMath::Max(Config.UniformScaleRange.X, Config.UniformScaleRange.Y);
		const float UniformScale = RandomStream.FRandRange(FMath::Max(0.01f, ScaleMin), FMath::Max(0.01f, ScaleMax));
		const FRotator Rotation(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
		const FTransform WorldTransform(Rotation, Location, FVector(UniformScale));

		const int32 InstanceIndex = Component->AddInstance(WorldTransform, true);
		if (InstanceIndex == INDEX_NONE)
		{
			continue;
		}

		RegisterInstance(INDEX_NONE, Component, InstanceIndex, WorldTransform, TArray<FGridVector>{ CellPos });
		++OutSpawnedInstances;
		bSpawnedAny = true;
	}

	return bSpawnedAny;
}

bool UCityFlowLandscapeDecorationManager::SampleGroundGrassScore(
	const TArray<FColor>& GroundColorPixels,
	int32 TextureWidth,
	int32 TextureHeight,
	const FVector& WorldLocation,
	const FCityFlowGrassCoverageConfig& Config,
	float& OutGrassScore,
	float& OutGreenRatio) const
{
	OutGrassScore = 0.0f;
	OutGreenRatio = 0.0f;
	if (GroundColorPixels.Num() == 0 || TextureWidth <= 0 || TextureHeight <= 0)
	{
		return false;
	}

	const float U = WorldLocation.X * Config.MaterialTile.X + Config.MaterialOffset.X;
	const float V = WorldLocation.Y * Config.MaterialTile.Y + Config.MaterialOffset.Y;
	const float WrappedU = U - FMath::FloorToFloat(U);
	const float WrappedV = V - FMath::FloorToFloat(V);
	const int32 PixelX = FMath::Clamp(FMath::FloorToInt(WrappedU * TextureWidth), 0, TextureWidth - 1);
	const int32 PixelY = FMath::Clamp(FMath::FloorToInt(WrappedV * TextureHeight), 0, TextureHeight - 1);
	const int32 PixelIndex = PixelY * TextureWidth + PixelX;
	if (!GroundColorPixels.IsValidIndex(PixelIndex))
	{
		return false;
	}

	const FColor Pixel = GroundColorPixels[PixelIndex];

	const float R = Pixel.R / 255.0f;
	const float G = Pixel.G / 255.0f;
	const float GreenRatio = G / FMath::Max(R, 0.001f);
	OutGreenRatio = GreenRatio;
	if (GreenRatio < Config.GreenRatioMin)
	{
		return true;
	}

	const float FullRatio = FMath::Max(Config.GreenRatioPivot, Config.GreenRatioMin + 0.05f);
	const float RatioT = FMath::Clamp((GreenRatio - Config.GreenRatioMin) / FMath::Max(FullRatio - Config.GreenRatioMin, 0.001f), 0.0f, 1.0f);
	const float StrongT = RatioT * RatioT;
	OutGrassScore = FMath::Lerp(FMath::Clamp(Config.DryGrassRatio, 0.0f, 1.0f), 1.0f, StrongT);
	return true;
}

bool UCityFlowLandscapeDecorationManager::TrySpawnDecorationInCell(
	const FGridVector& CellPos,
	const TArray<FCityFlowLandscapeDecorationConfig>& Configs,
	FRandomStream& RandomStream)
{
	const int32 ConfigIndex = PickConfigIndex(Configs, RandomStream);
	if (!Configs.IsValidIndex(ConfigIndex))
	{
		return false;
	}

	const FCityFlowLandscapeDecorationConfig& Config = Configs[ConfigIndex];
	if (RandomStream.FRand() > Config.SpawnChance)
	{
		return false;
	}

	UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	UStaticMesh* Mesh = Config.Mesh.LoadSynchronous();
	if (!Mesh)
	{
		return false;
	}

	UHierarchicalInstancedStaticMeshComponent* Component = GetOrCreateComponent(ConfigIndex, Config);
	if (!Component)
	{
		return false;
	}

	const float CellSize = GM->GetCellSize();
	const float OffsetRange = FMath::Clamp(Config.CellRandomOffset, 0.0f, 0.49f) * CellSize;
	FVector Location = GM->GridToWorld(CellPos);
	Location.X += RandomStream.FRandRange(-OffsetRange, OffsetRange);
	Location.Y += RandomStream.FRandRange(-OffsetRange, OffsetRange);
	Location.Z += Config.ZOffset;

	const float ScaleMin = FMath::Min(Config.UniformScaleRange.X, Config.UniformScaleRange.Y);
	const float ScaleMax = FMath::Max(Config.UniformScaleRange.X, Config.UniformScaleRange.Y);
	const float UniformScale = RandomStream.FRandRange(FMath::Max(0.01f, ScaleMin), FMath::Max(0.01f, ScaleMax));
	const FRotator Rotation(0.0f, RandomStream.FRandRange(0.0f, 360.0f), 0.0f);
	const FTransform WorldTransform(Rotation, Location, FVector(UniformScale));

	TArray<FGridVector> CoveredCells;
	if (!CalculateFootprintCells(Mesh, WorldTransform, Config, CoveredCells))
	{
		return false;
	}

	if (!AreFootprintCellsValid(CoveredCells, Config))
	{
		return false;
	}

	const int32 InstanceIndex = Component->AddInstance(WorldTransform, true);
	if (InstanceIndex == INDEX_NONE)
	{
		return false;
	}

	RegisterInstance(ConfigIndex, Component, InstanceIndex, WorldTransform, CoveredCells);
	return true;
}

int32 UCityFlowLandscapeDecorationManager::PickConfigIndex(
	const TArray<FCityFlowLandscapeDecorationConfig>& Configs,
	FRandomStream& RandomStream) const
{
	float TotalWeight = 0.0f;
	for (const FCityFlowLandscapeDecorationConfig& Config : Configs)
	{
		if (!Config.Mesh.IsNull())
		{
			TotalWeight += FMath::Max(0.0f, Config.SpawnWeight);
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	float Pick = RandomStream.FRandRange(0.0f, TotalWeight);
	for (int32 i = 0; i < Configs.Num(); ++i)
	{
		const FCityFlowLandscapeDecorationConfig& Config = Configs[i];
		if (Config.Mesh.IsNull())
		{
			continue;
		}

		Pick -= FMath::Max(0.0f, Config.SpawnWeight);
		if (Pick <= 0.0f)
		{
			return i;
		}
	}

	return Configs.Num() - 1;
}

bool UCityFlowLandscapeDecorationManager::CalculateFootprintCells(
	const UStaticMesh* Mesh,
	const FTransform& WorldTransform,
	const FCityFlowLandscapeDecorationConfig& Config,
	TArray<FGridVector>& OutCells) const
{
	OutCells.Reset();

	const UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized() || !Mesh)
	{
		return false;
	}

	const FBox LocalBox = Mesh->GetBoundingBox();
	if (!LocalBox.IsValid)
	{
		return false;
	}

	FVector2D MinXY(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D MaxXY(TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest());

	for (int32 X = 0; X < 2; ++X)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 Z = 0; Z < 2; ++Z)
			{
				const FVector LocalCorner(
					X == 0 ? LocalBox.Min.X : LocalBox.Max.X,
					Y == 0 ? LocalBox.Min.Y : LocalBox.Max.Y,
					Z == 0 ? LocalBox.Min.Z : LocalBox.Max.Z);

				const FVector WorldCorner = WorldTransform.TransformPosition(LocalCorner);
				MinXY.X = FMath::Min(MinXY.X, WorldCorner.X);
				MinXY.Y = FMath::Min(MinXY.Y, WorldCorner.Y);
				MaxXY.X = FMath::Max(MaxXY.X, WorldCorner.X);
				MaxXY.Y = FMath::Max(MaxXY.Y, WorldCorner.Y);
			}
		}
	}

	const float CellSize = GM->GetCellSize();
	const float Padding = FMath::Max(0.0f, Config.FootprintPaddingCells) * CellSize;
	MinXY -= FVector2D(Padding, Padding);
	MaxXY += FVector2D(Padding, Padding);

	const FVector GridOrigin = GM->GetGridOrigin();
	const float GridMinX = GridOrigin.X - CellSize * 0.5f;
	const float GridMinY = GridOrigin.Y - CellSize * 0.5f;

	const int32 MinCellX = FMath::FloorToInt((MinXY.X - GridMinX) / CellSize);
	const int32 MinCellY = FMath::FloorToInt((MinXY.Y - GridMinY) / CellSize);
	const int32 MaxCellX = FMath::FloorToInt((MaxXY.X - GridMinX) / CellSize);
	const int32 MaxCellY = FMath::FloorToInt((MaxXY.Y - GridMinY) / CellSize);

	if (Config.bRequireFootprintInsideGrid)
	{
		if (MinCellX < 0 || MinCellY < 0 || MaxCellX >= GM->GetGridWidth() || MaxCellY >= GM->GetGridHeight())
		{
			return false;
		}
	}

	const int32 ClampedMinX = FMath::Clamp(MinCellX, 0, GM->GetGridWidth() - 1);
	const int32 ClampedMinY = FMath::Clamp(MinCellY, 0, GM->GetGridHeight() - 1);
	const int32 ClampedMaxX = FMath::Clamp(MaxCellX, 0, GM->GetGridWidth() - 1);
	const int32 ClampedMaxY = FMath::Clamp(MaxCellY, 0, GM->GetGridHeight() - 1);

	for (int32 Y = ClampedMinY; Y <= ClampedMaxY; ++Y)
	{
		for (int32 X = ClampedMinX; X <= ClampedMaxX; ++X)
		{
			OutCells.Add(FGridVector(X, Y));
		}
	}

	return OutCells.Num() > 0;
}

bool UCityFlowLandscapeDecorationManager::AreFootprintCellsValid(
	const TArray<FGridVector>& Cells,
	const FCityFlowLandscapeDecorationConfig& Config) const
{
	const UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return false;
	}

	const UCityFlowRiverManager* RiverManager = GetWorld()
		? GetWorld()->GetSubsystem<UCityFlowRiverManager>()
		: nullptr;

	for (const FGridVector& Cell : Cells)
	{
		if (!GM->IsValidGridPos(Cell))
		{
			return false;
		}

		if (Config.bRejectOccupiedFootprint && GM->GetCellType(Cell) != ECellType::Empty)
		{
			return false;
		}

		if (RiverManager && RiverManager->IsRiverOrBankCell(Cell))
		{
			return false;
		}
	}

	return true;
}

void UCityFlowLandscapeDecorationManager::RegisterInstance(
	int32 ConfigIndex,
	UHierarchicalInstancedStaticMeshComponent* Component,
	int32 InstanceIndex,
	const FTransform& WorldTransform,
	const TArray<FGridVector>& CoveredCells)
{
	const int32 InstanceId = NextInstanceId++;

	FCityFlowLandscapeInstanceRecord Record;
	Record.ConfigIndex = ConfigIndex;
	Record.InstanceIndex = InstanceIndex;
	Record.bAlive = true;
	Record.WorldTransform = WorldTransform;
	Record.Component = Component;
	Record.CoveredCells = CoveredCells;

	InstanceRecords.Add(InstanceId, Record);

	for (const FGridVector& Cell : CoveredCells)
	{
		CellToInstanceIds.FindOrAdd(Cell).Add(InstanceId);
	}

	++LiveInstanceCount;
}

void UCityFlowLandscapeDecorationManager::ClearDecorationInstance(int32 InstanceId)
{
	FCityFlowLandscapeInstanceRecord* Record = InstanceRecords.Find(InstanceId);
	if (!Record || !Record->bAlive)
	{
		return;
	}

	if (UHierarchicalInstancedStaticMeshComponent* Component = Record->Component.Get())
	{
		if (Record->InstanceIndex != INDEX_NONE)
		{
			FTransform HiddenTransform = Record->WorldTransform;
			HiddenTransform.SetScale3D(FVector::ZeroVector);
			Component->UpdateInstanceTransform(Record->InstanceIndex, HiddenTransform, true, true, true);
		}
	}

	for (const FGridVector& Cell : Record->CoveredCells)
	{
		if (TSet<int32>* CellInstanceIds = CellToInstanceIds.Find(Cell))
		{
			CellInstanceIds->Remove(InstanceId);
			if (CellInstanceIds->Num() == 0)
			{
				CellToInstanceIds.Remove(Cell);
			}
		}
	}

	Record->bAlive = false;
	Record->CoveredCells.Empty();
	LiveInstanceCount = FMath::Max(0, LiveInstanceCount - 1);
}

void UCityFlowLandscapeDecorationManager::ClearDecorationIds(const TSet<int32>& InstanceIds)
{
	for (int32 InstanceId : InstanceIds)
	{
		ClearDecorationInstance(InstanceId);
	}
}

UGridManager* UCityFlowLandscapeDecorationManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
