#include "Grid/MeshGridPlaceableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

AMeshGridPlaceableActor::AMeshGridPlaceableActor()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
}

void AMeshGridPlaceableActor::OnEnterPreview_Implementation()
{
	Super::OnEnterPreview_Implementation();

	if (!MeshComponent || !PreviewMaterial)
	{
		return;
	}

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	OriginalMaterials.SetNum(NumMaterials);
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		OriginalMaterials[i] = MeshComponent->GetMaterial(i);
	}

	ApplyMaterialToAllSlots(PreviewMaterial);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMeshGridPlaceableActor::OnEnterPlaced_Implementation()
{
	Super::OnEnterPlaced_Implementation();

	if (!MeshComponent)
	{
		return;
	}

	RestoreOriginalMaterials();
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AMeshGridPlaceableActor::OnPreviewValidChanged_Implementation(bool bValid)
{
	Super::OnPreviewValidChanged_Implementation(bValid);

	if (!MeshComponent || !IsPreview())
	{
		return;
	}

	if (bValid)
	{
		if (PreviewMaterial)
		{
			ApplyMaterialToAllSlots(PreviewMaterial);
		}
	}
	else
	{
		if (InvalidPreviewMaterial)
		{
			ApplyMaterialToAllSlots(InvalidPreviewMaterial);
		}
	}
}

void AMeshGridPlaceableActor::ApplyMaterialToAllSlots(UMaterialInterface* Material)
{
	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		MeshComponent->SetMaterial(i, Material);
	}
}

void AMeshGridPlaceableActor::RestoreOriginalMaterials()
{
	for (int32 i = 0; i < OriginalMaterials.Num(); ++i)
	{
		if (OriginalMaterials[i])
		{
			MeshComponent->SetMaterial(i, OriginalMaterials[i]);
		}
	}
	OriginalMaterials.Empty();
}

void AMeshGridPlaceableActor::CaptureOriginalMaterials()
{
	if (!MeshComponent)
	{
		return;
	}

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	OriginalMaterials.SetNum(NumMaterials);
	for (int32 i = 0; i < NumMaterials; ++i)
	{
		OriginalMaterials[i] = MeshComponent->GetMaterial(i);
	}
}
