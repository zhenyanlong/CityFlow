#pragma once

#include "CoreMinimal.h"
#include "Grid/CityFlowGridTypes.h"
#include "CityFlowLandscapeDecorationTypes.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ECityFlowLandscapeDecorationKind : uint8
{
	GrassCluster UMETA(DisplayName = "Grass Cluster"),
	Tree         UMETA(DisplayName = "Tree"),
	Rock         UMETA(DisplayName = "Rock"),
	Mountain     UMETA(DisplayName = "Mountain")
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowLandscapeDecorationConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	ECityFlowLandscapeDecorationKind Kind = ECityFlowLandscapeDecorationKind::GrassCluster;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration", meta = (ClampMin = "0.0"))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration", meta = (ClampMin = "0.01"))
	FVector2D UniformScaleRange = FVector2D(0.8f, 1.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float CellRandomOffset = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	float ZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration", meta = (ClampMin = "0.0"))
	float FootprintPaddingCells = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	bool bRequireFootprintInsideGrid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoration")
	bool bRejectOccupiedFootprint = true;
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowGrassCoverageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage")
	TSoftObjectPtr<UStaticMesh> GrassMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage")
	TSoftObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage")
	TSoftObjectPtr<UTexture2D> GroundColorTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage", meta = (ClampMin = "0"))
	int32 DensityPerCell = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float GreenRatioPivot = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float GreenRatioMin = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass Coverage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DryGrassRatio = 0.03f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced")
	FVector2D MaterialOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced")
	FVector2D MaterialTile = FVector2D(0.005f, 0.005f);

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced", meta = (ClampMin = "0.01"))
	FVector2D UniformScaleRange = FVector2D(0.8f, 1.2f);

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced", meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float CellRandomOffset = 0.48f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced")
	float ZOffset = 0.0f;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Grass Coverage|Advanced")
	bool bRejectOccupiedCells = true;
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FCityFlowLandscapeInstanceHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Landscape")
	int32 InstanceId = INDEX_NONE;

	bool IsValid() const
	{
		return InstanceId != INDEX_NONE;
	}
};

struct CITYFLOW_API FCityFlowLandscapeInstanceRecord
{
	int32 ConfigIndex = INDEX_NONE;
	int32 InstanceIndex = INDEX_NONE;
	bool bAlive = false;
	FTransform WorldTransform = FTransform::Identity;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
	TArray<FGridVector> CoveredCells;
};
