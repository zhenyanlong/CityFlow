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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLSystemGenerationStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLSystemGenerationStep, int32, RemainingBudget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLSystemGenerationFinished, bool, bAllBuildingsConnected);

UCLASS()
class CITYFLOW_API ULSystemManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LSystem")
	void StartGenerate();

	UFUNCTION(BlueprintCallable, Category = "LSystem")
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

	void ProcessGrowthStep();

	bool TryGrowAt(const FLSystemGrowthPoint& Point);

	ARoadTile* CreateRoadTile(const FGridVector& GridPos);

	ABuilding* FindNearestUnconnectedBuilding(const FGridVector& From) const;

	float GetAttractionScore(const FGridVector& From, ABuilding* Target, EGridDirection Dir) const;

	bool IsBuildingConnected(ABuilding* Building) const;

	bool AreAllBuildingsConnected() const;

	TArray<ABuilding*> GetAllBuildings() const;

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
	bool bIsGenerating = false;
	int32 RemainingBudget = 0;
};
