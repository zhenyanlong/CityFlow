#include "Vehicle/Actor/VehicleActor.h"
#include "Grid/Building.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

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

void AVehicleActor::SetMovementPlan(const FVehicleMovementPlan& Plan)
{
	MovementPlan = Plan;
	MovementPlan.Reset();

	if (MovementPlan.IsValid())
	{
		const FVehicleWaypoint* FirstWp = MovementPlan.GetCurrentWaypoint();
		if (FirstWp)
		{
			SetActorLocation(FirstWp->Position + FVector(0, 0, VehicleZOffset));
		}
		MovementState = EVehicleMovementState::Moving;
	}
	else
	{
		MovementState = EVehicleMovementState::Idle;
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
		TickMovementSimple(DeltaTime);
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

void AVehicleActor::TickMovementSimple(float DeltaTime)
{
	if (MovementPlan.IsComplete())
	{
		HandleArrival();
		return;
	}

	const FVehicleWaypoint* CurrentWp = MovementPlan.GetCurrentWaypoint();
	if (!CurrentWp)
	{
		HandleArrival();
		return;
	}

	const FVector TargetPos = CurrentWp->Position + FVector(0, 0, VehicleZOffset);
	const FVector CurrentPos = GetActorLocation();
	const FVector ToTarget = TargetPos - CurrentPos;
	const float Distance = ToTarget.Size();

	if (Distance <= WaypointReachedThreshold)
	{
		MovementPlan.Advance();
		if (MovementPlan.IsComplete())
		{
			HandleArrival();
			return;
		}
		return;
	}

	const float EffectiveSpeed = MoveSpeed * CurrentWp->Speed;
	const FVector MoveDir = ToTarget.GetSafeNormal();
	if (!MoveDir.IsNearlyZero())
	{
		VelocityDirection = MoveDir;
		VehicleRoot->SetWorldRotation(FRotator(0.0, MoveDir.Rotation().Yaw, 0.0));
	}

	FVector NewPos = CurrentPos + MoveDir * EffectiveSpeed * DeltaTime;

	if (FVector::Dist(NewPos, TargetPos) <= WaypointReachedThreshold)
	{
		NewPos = TargetPos;
	}

	SetActorLocation(NewPos);

	if (CurrentWp->bIsIntersectionEntry && Distance < WaypointReachedThreshold * 4.0f)
	{
		MovementState = EVehicleMovementState::WaitingIntersection;
		IntersectionWaitTimer = IntersectionWaitTime;
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

bool AVehicleActor::NeedsIntersectionLock(const FGridVector& IntersectionPos) const
{
	if (MovementPlan.IsComplete())
	{
		return false;
	}

	const FVehicleWaypoint* Wp = MovementPlan.GetCurrentWaypoint();
	if (!Wp)
	{
		return false;
	}

	return Wp->bIsIntersectionEntry;
}

void AVehicleActor::NotifyIntersectionCleared(const FGridVector& IntersectionPos)
{
	if (WaitingIntersectionPos == IntersectionPos)
	{
		if (MovementState == EVehicleMovementState::WaitingIntersection)
		{
			MovementState = EVehicleMovementState::Moving;
		}
	}
}
