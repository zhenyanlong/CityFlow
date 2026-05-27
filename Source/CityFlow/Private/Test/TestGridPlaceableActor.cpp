#include "Test/TestGridPlaceableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ATestGridPlaceableActor::ATestGridPlaceableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	BuildingSize = FVector2D(1, 1);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.5f));
	}
}
