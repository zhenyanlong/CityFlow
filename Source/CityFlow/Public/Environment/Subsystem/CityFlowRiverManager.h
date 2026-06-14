#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Environment/Types/CityFlowRiverTypes.h"
#include "CityFlowRiverManager.generated.h"

class UGridManager;
class UTextureRenderTarget2D;

UCLASS()
class CITYFLOW_API UCityFlowRiverManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "CityFlow|River")
	void GenerateRivers(int32 Seed = -1);

	UFUNCTION(BlueprintCallable, Category = "CityFlow|River")
	void ClearRivers();

	UFUNCTION(BlueprintPure, Category = "CityFlow|River")
	bool IsRiverCell(FGridVector CellPos) const;

	UFUNCTION(BlueprintPure, Category = "CityFlow|River")
	bool IsRiverOrBankCell(FGridVector CellPos) const;

	UFUNCTION(BlueprintPure, Category = "CityFlow|River")
	UTextureRenderTarget2D* GetRiverMaskRenderTarget() const { return RiverMaskRenderTarget; }

	UFUNCTION(BlueprintPure, Category = "CityFlow|River")
	const TArray<FCityFlowRiverPath>& GetRiverPaths() const { return RiverPaths; }

	UFUNCTION(BlueprintCallable, Category = "CityFlow|River")
	void ApplyRiverMaskToLandscapeMaterial();

private:
	FCityFlowRiverPath GenerateSingleRiver(FRandomStream& RandomStream) const;
	void RasterizeRiverPath(FCityFlowRiverPath& RiverPath);
	void DrawRiverMask();
	void SetLandscapeRiverMaskEnabled(float EnabledValue);

	FVector2D WorldToMaskPixel(const FVector& WorldPosition, FVector2D MaskSize) const;
	float DistancePointToSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B) const;
	FVector PickEdgePoint(int32 Edge, FRandomStream& RandomStream) const;

	UGridManager* GetGridManager() const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RiverMaskRenderTarget;

	UPROPERTY(Transient)
	TArray<FCityFlowRiverPath> RiverPaths;

	TSet<FGridVector> RiverCells;
	TSet<FGridVector> RiverBankCells;
};
