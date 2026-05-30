#include "Grid/RoadTile.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"

ARoadTile::ARoadTile()
{
	PlaceableType = EPlaceableType::Road;
}

void ARoadTile::BeginPlay()
{
	Super::BeginPlay();
}

void ARoadTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.RemoveDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ARoadTile::OnPlacedOnGrid_Implementation()
{
	Super::OnPlacedOnGrid_Implementation();

	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.AddDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	UpdateAppearance();
}

void ARoadTile::OnRemovedFromGrid_Implementation()
{
	UGridManager* GM = GetGridManager();
	if (GM)
	{
		GM->OnCellChanged.RemoveDynamic(this, &ARoadTile::OnGridCellChanged);
	}

	Super::OnRemovedFromGrid_Implementation();
}

void ARoadTile::OnGridCellChanged(FGridVector CellPos, const FGridCell& NewCell)
{
	if (CellPos == GridPosition)
	{
		UpdateAppearance();
	}
}

void ARoadTile::UpdatePreviewAppearance(const FGridVector& GridPos)
{
	if (!IsPreview() || !MeshComponent)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const uint8 PredictedMask = GM->CalculateConnectedMask(GridPos);

	UStaticMesh* FoundMesh = nullptr;
	float Yaw = 0.0f;
	FVector ScaleMult = FVector(1.0f, 1.0f, 1.0f);

	if (FindMeshConfig(PredictedMask, FoundMesh, Yaw, ScaleMult) && FoundMesh)
	{
		EnsureMeshMaterialsCached(FoundMesh);

		MeshComponent->SetStaticMesh(FoundMesh);

		const int32 RotCount = FMath::RoundToInt32(Yaw / 90.0f) % 4;
		if (RotCount % 2 == 1)
		{
			Swap(ScaleMult.X, ScaleMult.Y);
		}

		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));

		const float CellSize = GM->GetCellSize();
		const float BaseScale = (ReferenceCellSize > 0.0f) ? (CellSize / ReferenceCellSize) : 1.0f;
		SetActorScale3D(ScaleMult * BaseScale);

		UMaterialInterface* MaterialToApply = IsPreviewPlacementValid() ? PreviewMaterial : InvalidPreviewMaterial;
		if (MaterialToApply)
		{
			const int32 NumSlots = MeshComponent->GetNumMaterials();
			for (int32 i = 0; i < NumSlots; ++i)
			{
				MeshComponent->SetMaterial(i, MaterialToApply);
			}
		}
	}
}

void ARoadTile::UpdateAppearance()
{
	if (!IsPlacedOnGrid() || !MeshComponent)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const FGridCell& Cell = GM->GetCell(GridPosition);
	const uint8 Mask = Cell.ConnectedMask;

	UStaticMesh* FoundMesh = nullptr;
	float Yaw = 0.0f;
	FVector ScaleMult = FVector(1.0f, 1.0f, 1.0f);

	if (FindMeshConfig(Mask, FoundMesh, Yaw, ScaleMult) && FoundMesh)
	{
		EnsureMeshMaterialsCached(FoundMesh);

		MeshComponent->SetStaticMesh(FoundMesh);

		RestoreMeshMaterials(FoundMesh);

		const int32 RotCount = FMath::RoundToInt32(Yaw / 90.0f) % 4;
		if (RotCount % 2 == 1)
		{
			Swap(ScaleMult.X, ScaleMult.Y);
		}

		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));

		const float CellSize = GM->GetCellSize();
		const float BaseScale = (ReferenceCellSize > 0.0f) ? (CellSize / ReferenceCellSize) : 1.0f;
		SetActorScale3D(ScaleMult * BaseScale);
	}
}

bool ARoadTile::FindMeshConfig(uint8 Mask, UStaticMesh*& OutMesh, float& OutYaw, FVector& OutScaleMultiplier) const
{
	if (Mask == 0)
	{
		return false;
	}

	for (const FRoadMeshConfig& Config : RoadMeshConfigs)
	{
		uint8 Rotated = Config.CanonicalMask;
		for (int32 Rot = 0; Rot < 4; ++Rot)
		{
			if (Rotated == Mask)
			{
				OutMesh = Config.Mesh;
				OutYaw = static_cast<float>(Rot) * 90.0f;
				OutScaleMultiplier = Config.ScaleMultiplier;
				return true;
			}
			Rotated = RotateMask90CW(Rotated);
		}
	}

	return false;
}

uint8 ARoadTile::RotateMask90CW(uint8 Mask)
{
	uint8 Result = 0;
	if (Mask & 0x01) Result |= 0x08;
	if (Mask & 0x08) Result |= 0x02;
	if (Mask & 0x02) Result |= 0x04;
	if (Mask & 0x04) Result |= 0x01;
	return Result;
}

void ARoadTile::OnEnterPlaced_Implementation()
{
	Super::OnEnterPlaced_Implementation();

	UStaticMesh* CurrentMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	if (CurrentMesh)
	{
		RestoreMeshMaterials(CurrentMesh);
	}
}

void ARoadTile::OnPreviewValidChanged_Implementation(bool bValid)
{
}

void ARoadTile::EnsureMeshMaterialsCached(UStaticMesh* Mesh)
{
	if (!Mesh || MeshMaterialCache.Contains(Mesh))
	{
		return;
	}

	const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	Materials.Reserve(StaticMaterials.Num());
	for (const FStaticMaterial& SM : StaticMaterials)
	{
		Materials.Add(SM.MaterialInterface);
	}
	MeshMaterialCache.Add(Mesh, MoveTemp(Materials));
}

void ARoadTile::RestoreMeshMaterials(UStaticMesh* Mesh)
{
	if (!Mesh || !MeshComponent)
	{
		return;
	}

	const TArray<TObjectPtr<UMaterialInterface>>* Cached = MeshMaterialCache.Find(Mesh);
	if (!Cached)
	{
		return;
	}

	for (int32 i = 0; i < Cached->Num(); ++i)
	{
		if ((*Cached)[i])
		{
			MeshComponent->SetMaterial(i, (*Cached)[i]);
		}
	}
}
