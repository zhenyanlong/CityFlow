#pragma once

#include "CoreMinimal.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "RampageVehicle.generated.h"

/**
 * A vehicle that, instead of dying when its wait timeout expires,
 * enters a berserk "rampage" mode: ignores all forward probes,
 * drives at increased speed, and kills any vehicles in its path.
 */
UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API ARampageVehicle : public AVehicleActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	/** Speed multiplier applied in rampage mode (default 1.2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Berserk")
	float RampageSpeedMultiplier = 1.2f;

	/** Red flash frequency while in rampage mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Berserk")
	float RampageFlashFrequency = 18.0f;

protected:
	// ===== Overrides =====

	/** When wait timeout expires, enter rampage mode instead of dying. */
	virtual void HandleWaitTimeout() override;

	/** In rampage mode, apply the rampage speed multiplier. */
	virtual float GetBerserkSpeedMultiplier() const override;
};
