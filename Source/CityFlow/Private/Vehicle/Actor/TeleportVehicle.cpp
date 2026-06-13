#include "Vehicle/Actor/TeleportVehicle.h"
#include "Vehicle/Subsystem/VehicleManager.h"
#include "Vehicle/Subsystem/CityFlowDeveloperSettings.h"
#include "Grid/GridManager.h"
#include "Grid/RoadTile.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

void ATeleportVehicle::HandleWaitTimeout()
{
	if (!PathSpline || PathSpline->GetNumberOfSplinePoints() < 2)
	{
		TotalStopTime = 0.0f;
		return;
	}

	const float SplineLength = PathSpline->GetSplineLength();
	if (SplineLength <= 0.0f)
	{
		TotalStopTime = 0.0f;
		return;
	}

	const FVector OldLocation = GetActorLocation();
	if (TeleportBeforeVFX)
	{
		UNiagaraComponent* VFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), TeleportBeforeVFX, OldLocation, GetActorRotation(),
			FVector::OneVector, true, true, ENCPoolMethod::None);
		if (VFXComp && !TeleportVFXScaleParamName.IsNone())
		{
			VFXComp->SetVariableFloat(TeleportVFXScaleParamName, TeleportVFXScale);
		}
	}

	const float MinDistance = FMath::Max(0.0f, FMath::Min(TeleportMinDistance, TeleportMaxDistance));
	const float MaxDistance = FMath::Max(MinDistance, FMath::Max(TeleportMinDistance, TeleportMaxDistance));
	const float TeleportDistance = FMath::FRandRange(MinDistance, MaxDistance);
	CurrentSplineDistance = FMath::Clamp(CurrentSplineDistance + TeleportDistance, 0.0f, SplineLength);

	const FVector NewPos = PathSpline->GetLocationAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);
	const FVector MoveDir = PathSpline->GetDirectionAtDistanceAlongSpline(CurrentSplineDistance, ESplineCoordinateSpace::World);

	if (!MoveDir.IsNearlyZero())
	{
		VelocityDirection = MoveDir;
		VehicleRoot->SetWorldRotation(MoveDir.Rotation());
	}

	SetActorLocation(NewPos + FVector(0, 0, VehicleZOffset));

	if (TeleportAfterVFX)
	{
		UNiagaraComponent* VFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), TeleportAfterVFX, GetActorLocation(), GetActorRotation(),
			FVector::OneVector, true, true, ENCPoolMethod::None);
		if (VFXComp && !TeleportVFXScaleParamName.IsNone())
		{
			VFXComp->SetVariableFloat(TeleportVFXScaleParamName, TeleportVFXScale);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UGridManager* GridMgr = World->GetSubsystem<UGridManager>())
		{
			PreviousGridPosition = GridMgr->WorldToGrid(NewPos);
		}
	}

	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	TotalStopTime = 0.0f;
	CongestionWaitTime = 0.0f;
	bFrontVehicleTooClose = false;
	MovementState = EVehicleMovementState::Moving;
	CurrentSpeed = 0.0f;

	if (FlashMaterialInstance)
	{
		FlashMaterialInstance->SetScalarParameterValue(TEXT("FlashIntensity"), 0.0f);
	}

	KillOverlappingVehicles(TeleportOverlapRadius);

	const UCityFlowDeveloperSettings* Settings = UCityFlowDeveloperSettings::Get();
	if (GEngine && Settings && Settings->bDebugVehicleAbilities)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Purple,
			FString::Printf(TEXT("[%s] Teleported forward %.0f cm"), *GetName(), TeleportDistance));
	}

	if (UWorld* World = GetWorld())
	{
		if (UVehicleManager* VehicleManager = World->GetSubsystem<UVehicleManager>())
		{
			VehicleManager->NotifyVehicleAbilityActivated(this, EVehicleAbilityAlertType::Teleport);
		}
	}
}
