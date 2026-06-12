#include "Vehicle/Actor/RampageVehicle.h"
#include "Grid/RoadTile.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogRampageVehicle, Log, All);

void ARampageVehicle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bBerserk || !VehicleMesh)
	{
		return;
	}

	if (!FlashMaterialInstance && VehicleMesh->GetMaterial(0))
	{
		FlashMaterialInstance = VehicleMesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (FlashMaterialInstance && GetWorld())
	{
		const float TimeSeconds = GetWorld()->GetTimeSeconds();
		const float Intensity = (FMath::Sin(TimeSeconds * RampageFlashFrequency * 2.0f * PI) + 1.0f) * 0.5f;
		FlashMaterialInstance->SetScalarParameterValue(TEXT("FlashIntensity"), Intensity);
	}
}

void ARampageVehicle::HandleWaitTimeout()
{
	// Instead of dying, enter berserk rampage mode
	bBerserk = true;

	// Release all intersection reservations — we don't respect locks in rampage mode
	for (const TWeakObjectPtr<ARoadTile>& WeakTile : ReservedIntersections)
	{
		if (ARoadTile* Tile = WeakTile.Get())
		{
			Tile->ReleaseVehicleFromAllTables(this);
		}
	}
	ReservedIntersections.Empty();
	PassedIntersections.Empty();

	// Reset accumulated timers
	TotalStopTime = 0.0f;
	CongestionWaitTime = 0.0f;

	// Set state back to Moving so we drive through
	MovementState = EVehicleMovementState::Moving;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
			FString::Printf(TEXT("[%s] RAMPAGE MODE ACTIVATED!"), *GetName()));
	}
}

float ARampageVehicle::GetBerserkSpeedMultiplier() const
{
	return bBerserk ? RampageSpeedMultiplier : 1.0f;
}
