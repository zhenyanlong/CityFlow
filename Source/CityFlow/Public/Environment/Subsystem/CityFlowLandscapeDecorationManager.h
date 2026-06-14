#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Environment/Types/CityFlowLandscapeDecorationTypes.h"
#include "CityFlowLandscapeDecorationManager.generated.h"

class AGridPlaceableActor;
class UGridManager;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UTexture2D;

UCLASS()
class CITYFLOW_API UCityFlowLandscapeDecorationManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Landscape")
	void GenerateDecorations(int32 Seed = -1);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Landscape")
	void ClearDecorations();

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Landscape")
	void ClearDecorationsForCell(FGridVector CellPos, int32 ExtraPaddingCells = 0);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|Landscape")
	void ClearDecorationsForCells(const TArray<FGridVector>& Cells, int32 ExtraPaddingCells = 0);

	UFUNCTION(BlueprintPure, Category = "CityFlow|Landscape")
	bool HasLiveDecorations() const { return LiveInstanceCount > 0; }

private:
	UFUNCTION()
	void HandleCellChanged(FGridVector CellPos, const FGridCell& NewCell);

	UFUNCTION()
	void HandleGridPlaced(AGridPlaceableActor* PlacedActor);

	void BindGridEvents();
	void UnbindGridEvents();

	bool EnsureRootActor();
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateComponent(int32 ConfigIndex, const FCityFlowLandscapeDecorationConfig& Config);
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateGrassCoverageComponent(const FCityFlowGrassCoverageConfig& Config);

	void GenerateGrassCoverage(const FCityFlowGrassCoverageConfig& Config, FRandomStream& RandomStream);
	bool ReadGroundColorPixels(
		UTexture2D* GroundColorTexture,
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight) const;
	bool TrySpawnGrassInCell(
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
		float& OutMaxObservedRatio);
	bool SampleGroundGrassScore(
		const TArray<FColor>& GroundColorPixels,
		int32 TextureWidth,
		int32 TextureHeight,
		const FVector& WorldLocation,
		const FCityFlowGrassCoverageConfig& Config,
		float& OutGrassScore,
		float& OutGreenRatio) const;

	bool TrySpawnDecorationInCell(
		const FGridVector& CellPos,
		const TArray<FCityFlowLandscapeDecorationConfig>& Configs,
		FRandomStream& RandomStream);

	int32 PickConfigIndex(const TArray<FCityFlowLandscapeDecorationConfig>& Configs, FRandomStream& RandomStream) const;

	bool CalculateFootprintCells(
		const UStaticMesh* Mesh,
		const FTransform& WorldTransform,
		const FCityFlowLandscapeDecorationConfig& Config,
		TArray<FGridVector>& OutCells) const;

	bool AreFootprintCellsValid(
		const TArray<FGridVector>& Cells,
		const FCityFlowLandscapeDecorationConfig& Config) const;

	void RegisterInstance(
		int32 ConfigIndex,
		UHierarchicalInstancedStaticMeshComponent* Component,
		int32 InstanceIndex,
		const FTransform& WorldTransform,
		const TArray<FGridVector>& CoveredCells);

	void ClearDecorationInstance(int32 InstanceId);
	void ClearDecorationIds(const TSet<int32>& InstanceIds);

	UGridManager* GetGridManager() const;

	TWeakObjectPtr<AActor> RootActor;
	TArray<TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ComponentsByConfigIndex;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrassCoverageComponent;
	TMap<int32, FCityFlowLandscapeInstanceRecord> InstanceRecords;
	TMap<FGridVector, TSet<int32>> CellToInstanceIds;

	bool bGridEventsBound = false;
	int32 NextInstanceId = 1;
	int32 LiveInstanceCount = 0;
};
