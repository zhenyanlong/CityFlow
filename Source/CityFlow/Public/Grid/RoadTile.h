#pragma once

#include "CoreMinimal.h"
#include "Grid/MeshGridPlaceableActor.h"
#include "RoadTile.generated.h"

USTRUCT(BlueprintType)
struct CITYFLOW_API FRoadMeshConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	uint8 CanonicalMask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector ScaleMultiplier = FVector(1.0f, 1.0f, 1.0f);
};

UCLASS()
class CITYFLOW_API ARoadTile : public AMeshGridPlaceableActor
{
	GENERATED_BODY()

public:
	ARoadTile();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<FRoadMeshConfig> RoadMeshConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float ReferenceCellSize = 200.0f;

	UFUNCTION(BlueprintCallable, Category = "Road")
	void UpdateAppearance();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPlacedOnGrid_Implementation() override;
	virtual void OnRemovedFromGrid_Implementation() override;
	virtual ECellType GetPlacementCellType() const override { return ECellType::Road; }

private:
	UFUNCTION()
	void OnGridCellChanged(FGridVector CellPos, const FGridCell& NewCell);

	bool FindMeshConfig(uint8 Mask, UStaticMesh*& OutMesh, float& OutYaw, FVector& OutScaleMultiplier) const;
	static uint8 RotateMask90CW(uint8 Mask);

};
