#include "Test/TestGridPlaceableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

ATestGridPlaceableActor::ATestGridPlaceableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BuildingSize = FVector2D(1, 1);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.5f));
	}
}

void ATestGridPlaceableActor::BeginPreview_Implementation()
{
	Super::BeginPreview_Implementation();

	if (!MeshComponent || !MeshComponent->GetMaterial(0))
	{
		return;
	}

	PreviewMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	if (PreviewMaterial)
	{
		PreviewMaterial->SetVectorParameterValue(TEXT("BaseColor"), FVector(PreviewColor.R, PreviewColor.G, PreviewColor.B));
		PreviewMaterial->SetScalarParameterValue(TEXT("Opacity"), PreviewOpacity);
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATestGridPlaceableActor::OnPlacedOnGrid_Implementation()
{
	Super::OnPlacedOnGrid_Implementation();

	if (PreviewMaterial)
	{
		PreviewMaterial->SetVectorParameterValue(TEXT("BaseColor"), FVector(PlacedColor.R, PlacedColor.G, PlacedColor.B));
		PreviewMaterial->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
	}
}
