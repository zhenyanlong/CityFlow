#include "Grid/RoadTile.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"

ARoadTile::ARoadTile()
{
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
