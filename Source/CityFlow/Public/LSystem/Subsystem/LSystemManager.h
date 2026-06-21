#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Grid/CityFlowGridTypes.h"
#include "LSystemManager.generated.h"

class ARoadTile;
class ABuilding;

USTRUCT()
struct FLSystemGrowthPoint
{
	GENERATED_BODY()

	FGridVector Position;
	EGridDirection Direction;

	FLSystemGrowthPoint() = default;
	FLSystemGrowthPoint(const FGridVector& InPos, EGridDirection InDir)
		: Position(InPos), Direction(InDir) {}

	bool operator==(const FLSystemGrowthPoint& Other) const
	{
		return Position == Other.Position && Direction == Other.Direction;
	}

	friend uint32 GetTypeHash(const FLSystemGrowthPoint& Point)
	{
		return HashCombine(GetTypeHash(Point.Position), GetTypeHash(static_cast<uint8>(Point.Direction)));
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLSystemGenerationStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLSystemGenerationStep, int32, RemainingBudget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLSystemGenerationFinished, bool, bAllBuildingsConnected);

/**
 * Connectivity-first procedural road assistant.
 *
 * Generation is deliberately split into two layers. A reserved connection plan
 * first guarantees that affordable building-to-network paths are not consumed
 * by decorative growth. The remaining budget may then be used by local,
 * L-system-inspired growth rules. All mutations still pass through GridManager,
 * so budget accounting and road connection masks remain authoritative.
 */
UCLASS()
class CITYFLOW_API ULSystemManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	/** Builds a connection plan and starts timer-driven generation. */
	void StartGenerate();

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	/** Cancels pending timers without removing roads that were already committed. */
	void AbortGeneration();

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetRoadTileClass(TSubclassOf<ARoadTile> InClass);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	TSubclassOf<ARoadTile> GetRoadTileClass() const { return RoadTileClass; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetBranchBudget(int32 NewBudget);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	int32 GetBranchBudget() const { return BranchBudget; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetGrowthInterval(float NewInterval);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	float GetGrowthInterval() const { return GrowthInterval; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetBranchProbability(float NewProbability);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	float GetBranchProbability() const { return BranchProbability; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetAttractionStrength(float NewStrength);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	float GetAttractionStrength() const { return AttractionStrength; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetStraightExtendLength(int32 NewLength);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	int32 GetStraightExtendLength() const { return StraightExtendLength; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void SetMinBranchSpacing(int32 NewSpacing);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	int32 GetMinBranchSpacing() const { return MinBranchSpacing; }

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void AddBudget(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "LSystem")
	bool IsGenerating() const { return bIsGenerating; }

	UFUNCTION(BlueprintPure, Category = "LSystem")
	int32 GetRemainingBudget() const { return RemainingBudget; }

	/** True only when every building doorway is attached to the same road component. */
	UFUNCTION(BlueprintPure, Category = "LSystem")
	bool AreAllBuildingsConnected() const;

	UPROPERTY(BlueprintAssignable, Category = "LSystem|Events")
	FOnLSystemGenerationStarted OnGenerationStarted;

	UPROPERTY(BlueprintAssignable, Category = "LSystem|Events")
	FOnLSystemGenerationStep OnGenerationStep;

	UPROPERTY(BlueprintAssignable, Category = "LSystem|Events")
	FOnLSystemGenerationFinished OnGenerationFinished;

private:
	class UGridManager* GetGridManager() const;

	void CollectStartPoints();
	void CollectStartPointsFromRoads();
	void CollectStartPointsFromBuildings();
	void EnqueueGrowthPoint(const FLSystemGrowthPoint& Point);

	void ProcessGrowthStep();
	bool ProcessConnectionPlanStep();

	bool TryGrowAt(const FLSystemGrowthPoint& Point);

	ARoadTile* CreateRoadTile(const FGridVector& GridPos);

	ABuilding* FindNearestUnconnectedBuilding(const FGridVector& From) const;

	float GetAttractionScore(const FGridVector& From, ABuilding* Target, EGridDirection Dir) const;

	bool IsBuildingConnected(ABuilding* Building) const;

	TArray<ABuilding*> GetAllBuildings() const;
	TSet<FGridVector> GetPrimaryRoadComponent() const;
	TSet<FGridVector> FloodRoadComponent(const FGridVector& Seed, const TSet<FGridVector>& RoadCells) const;
	bool IsBuildingConnectedToComponent(const ABuilding* Building, const TSet<FGridVector>& Component) const;
	void ExpandNetworkThroughRoads(TSet<FGridVector>& Network, const TSet<FGridVector>& RoadCells) const;
	bool FindPathToNetwork(const ABuilding* Building, const TSet<FGridVector>& Network, TArray<FGridVector>& OutPath) const;
	void BuildConnectionPlan();
	int32 CountPendingConnectionCost() const;
	int32 GetGenerationBudgetRemaining() const;

	bool CanGrowInDirection(const FGridVector& Pos, EGridDirection Dir) const;
	bool IsSideBranchValid(const FGridVector& Pos, EGridDirection Dir) const;

	static int32 PopCount(uint8 Mask);
	static bool IsStraightRoad(uint8 Mask);
	static bool IsDeadEnd(uint8 Mask);
	static EGridDirection TurnLeft(EGridDirection Dir);
	static EGridDirection TurnRight(EGridDirection Dir);
	static EGridDirection OppositeDirection(EGridDirection Dir);

	void FinishGeneration(bool bAllConnected);

	TSubclassOf<ARoadTile> RoadTileClass;
	int32 BranchBudget = 50;
	float GrowthInterval = 0.1f;
	float BranchProbability = 0.6f;
	float AttractionStrength = 0.7f;
	int32 StraightExtendLength = 3;
	int32 MinBranchSpacing = 3;

	FTimerHandle GrowthTimerHandle;
	TArray<FLSystemGrowthPoint> PendingGrowthPoints;
	TSet<FLSystemGrowthPoint> QueuedGrowthPoints;
	TArray<FGridVector> PendingConnectionCells;
	int32 NextConnectionCellIndex = 0;
	int32 CellsPlacedThisGeneration = 0;
	FRandomStream GenerationRandom;
	bool bIsGenerating = false;
	int32 RemainingBudget = 0;
};
