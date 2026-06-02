#include "Vehicle/Actor/VehicleActor.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Grid/GridManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVehicleActor, Log, All);

AVehicleActor::AVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	VehicleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleRoot"));
	SetRootComponent(VehicleRoot);

	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	VehicleMesh->SetupAttachment(VehicleRoot);
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetAbsolute(true, true, true);
	PathSpline->SetWorldLocation(FVector::ZeroVector);
	PathSpline->SetVisibility(false);
	PathSpline->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackCube(TEXT("/Engine/BasicShapes/Cube"));
	if (FallbackCube.Succeeded())
	{
		VehicleMesh->SetStaticMesh(FallbackCube.Object);
		VehicleMesh->SetRelativeScale3D(FVector(0.4f, 0.2f, 0.15f));
	}
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();
}

void AVehicleActor::SetSplinePath(const TArray<FVector>& WorldPoints)
{
	PathSpline->ClearSplinePoints();
	PathSpline->SetWorldLocation(FVector::ZeroVector);

	for (const FVector& Pt : WorldPoints)
	{
		PathSpline->AddSplineWorldPoint(Pt);
	}

	CurrentSplineDistance = 0.0f;
	MovementState = EVehicleMovementState::Moving;

	if (WorldPoints.Num() > 0)
	{
		SetActorLocation(WorldPoints[0] + FVector(0, 0, VehicleZOffset));
	}
}

void AVehicleActor::SetDestination(ABuilding* InDestination)
{
	Destination = InDestination;
}

void AVehicleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (MovementState)
	{
	case EVehicleMovementState::Moving:
		TickMovementSpline(DeltaTime);
		break;
	case EVehicleMovementState::WaitingIntersection:
		IntersectionWaitTimer -= DeltaTime;
		if (IntersectionWaitTimer <= 0.0f)
		{
			MovementState = EVehicleMovementState::Moving;
		}
		break;
	case EVehicleMovementState::WaitingCongestion:
		break;
	case EVehicleMovementState::Arrived:
	case EVehicleMovementState::Idle:
		break;
	}
}

void AVehicleActor::TickMovementSpline(float DeltaTime)
{
	if (PathSpline->GetNumberOfSplinePoints() < 2)
	{
		return;
	}

	const float SplineLength = PathSpline->GetSplineLength();
	if (CurrentSplineDistance >= SplineLength)
	{
		HandleArrival();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGridManager* GM = World->GetSubsystem<UGridManager>();
	UVehicleManager* VM = World->GetSubsystem<UVehicleManager>();

	if (GM && VM)
	{
		const float CellSize = GM->GetCellSize();
		const float LookAhead = CellSize * 0.5f;
		const float LookDist = FMath::Min(CurrentSplineDistance + LookAhead, SplineLength - 1.0f);

		const FVector AheadPos = PathSpline->GetLocationAtDistanceAlongSpline(LookDist, ESplineCoordinateSpace::World);
		const FGridVector AheadGrid = GM->WorldToGrid(AheadPos);
		const FGridVector CurrentGrid = GM->WorldToGrid(GetActorLocation());

		if (AheadGrid != CurrentGrid)
		{
			const FGridCell& AheadCell = GM->GetCell(AheadGrid);
			if (AheadCell.Type == ECellType::Road)
			{
				int32 ConnCount = 0;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Up)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Down)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Left)) ++ConnCount;
				if (AheadCell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;

				if (ConnCount >= 3 && VM->IsIntersectionLockedByOther(AheadGrid, this))
				{
					MovementState = EVehicleMovementState::WaitingIntersection;
					WaitingIntersectionPos = AheadGrid;
					IntersectionWaitTimer = IntersectionWaitTime;
					return;
				}
			}
		}
	}

	CurrentSplineDistance += MoveSpeed * DeltaTime;

	if (CurrentSplineDistance >= SplineLength)
	{
		CurrentSplineDistance = SplineLength;
		const FVector EndPos = PathSpline->GetLocationAtDistanceAlongSpline(SplineLength, ESplineCoordinateSpace::World);
		SetActorLocation(EndPos + FVector(0, 0, VehicleZOffset));
		HandleArrival();
		return;
	}

	const FVector NewPos = PathSpline->GetLocationAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);
	const FVector MoveDir = PathSpline->GetDirectionAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);

	if (!MoveDir.IsNearlyZero())
	{
		VelocityDirection = MoveDir;
		VehicleRoot->SetWorldRotation(MoveDir.Rotation());
	}

	SetActorLocation(NewPos + FVector(0, 0, VehicleZOffset));

	if (GM && VM)
	{
		const FGridVector NewGrid = GM->WorldToGrid(GetActorLocation());
		const FGridCell& Cell = GM->GetCell(NewGrid);
		if (Cell.Type == ECellType::Road)
		{
			int32 ConnCount = 0;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Up)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Down)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Left)) ++ConnCount;
			if (Cell.ConnectedMask & static_cast<uint8>(EGridDirection::Right)) ++ConnCount;
			if (ConnCount >= 3)
			{
				VM->AcquireIntersectionLock(NewGrid, this);
			}
		}
	}
}

void AVehicleActor::HandleArrival()
{
	MovementState = EVehicleMovementState::Arrived;
	VelocityDirection = FVector::ZeroVector;
	OnVehicleArrived.Broadcast(this);
}

void AVehicleActor::SetDebugColor(FLinearColor Color)
{
	DebugColor = Color;

	if (VehicleMesh && VehicleMesh->GetMaterial(0))
	{
		UMaterialInstanceDynamic* DynMat = VehicleMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	}
}

void AVehicleActor::SetWaitingForIntersection(bool bWaiting)
{
	if (bWaiting)
	{
		MovementState = EVehicleMovementState::WaitingIntersection;
		IntersectionWaitTimer = IntersectionWaitTime;
	}
	else
	{
		if (MovementState == EVehicleMovementState::WaitingIntersection)
		{
			MovementState = EVehicleMovementState::Moving;
		}
	}
}
