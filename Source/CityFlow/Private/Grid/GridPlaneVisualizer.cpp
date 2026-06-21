#include "Grid/GridPlaneVisualizer.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AGridPlaneVisualizer::AGridPlaneVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	PlaneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneComponent"));
	RootComponent = PlaneComponent;
	PlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaneComponent->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		PlaneComponent->SetStaticMesh(PlaneMesh.Object);
	}
}

void AGridPlaneVisualizer::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSetup)
	{
		SetupPlane();
	}
}

void AGridPlaneVisualizer::SetupPlane()
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	if (!GridMaterial)
	{
		return;
	}

	const float TotalWidth = GM->GetGridWidth() * GM->GetCellSize();
	const float TotalHeight = GM->GetGridHeight() * GM->GetCellSize();
	const FVector Origin = GM->GetGridOrigin();

	const FVector PlaneCenter(
		Origin.X + (GM->GetGridWidth() - 1) * GM->GetCellSize() * 0.5f,
		Origin.Y + (GM->GetGridHeight() - 1) * GM->GetCellSize() * 0.5f,
		Origin.Z + ZOffset
	);

	const float ScaleX = TotalWidth / kDefaultPlaneSize;
	const float ScaleY = TotalHeight / kDefaultPlaneSize;

	PlaneComponent->SetWorldLocation(PlaneCenter);
	PlaneComponent->SetWorldScale3D(FVector(ScaleX, ScaleY, 1.0f));

	DynamicMaterial = PlaneComponent->CreateDynamicMaterialInstance(0, GridMaterial);

	UpdateMaterialParams();

	bGridVisible = true;
	bPlaneSetup = true;
}

void AGridPlaneVisualizer::UpdateMaterialParams()
{
	if (!DynamicMaterial)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const FVector Origin = GM->GetGridOrigin();

	DynamicMaterial->SetScalarParameterValue(TEXT("CellSize"), GM->GetCellSize());
	DynamicMaterial->SetScalarParameterValue(TEXT("LineWidth"), LineWidth);
	DynamicMaterial->SetVectorParameterValue(TEXT("LineColor"), LineColor);
	// GridOrigin is the centre of cell (0,0); material grid lines belong on cell boundaries.
	DynamicMaterial->SetScalarParameterValue(TEXT("OriginX"), Origin.X - GM->GetCellSize() * 0.5f);
	DynamicMaterial->SetScalarParameterValue(TEXT("OriginY"), Origin.Y - GM->GetCellSize() * 0.5f);
}

void AGridPlaneVisualizer::SetGridVisible(bool bVisible)
{
	if (PlaneComponent)
	{
		PlaneComponent->SetVisibility(bVisible);
		bGridVisible = bVisible;
	}
}

UGridManager* AGridPlaneVisualizer::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
