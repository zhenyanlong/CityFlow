#pragma once

#include "CoreMinimal.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "TeleportVehicle.generated.h"

class UNiagaraSystem;

/**
 * A vehicle that teleports forward along its spline when its wait timeout expires.
 * If the destination overlaps other vehicles, those vehicles are killed.
 */
UCLASS(Blueprintable, BlueprintType)
class CITYFLOW_API ATeleportVehicle : public AVehicleActor
{
	GENERATED_BODY()

public:
	/** Minimum distance moved forward along the current spline when wait timeout expires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport", meta = (ClampMin = "0.0"))
	float TeleportMinDistance = 1200.0f;

	/** Maximum distance moved forward along the current spline when wait timeout expires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport", meta = (ClampMin = "0.0"))
	float TeleportMaxDistance = 3000.0f;

	/** Sphere radius used after teleport to find overlapping vehicles to kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport", meta = (ClampMin = "0.0"))
	float TeleportOverlapRadius = 120.0f;

	/** Niagara VFX spawned at the old position before teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport|VFX")
	TObjectPtr<UNiagaraSystem> TeleportBeforeVFX;

	/** Niagara VFX spawned at the new position after teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport|VFX")
	TObjectPtr<UNiagaraSystem> TeleportAfterVFX;

	/** Float value pushed to the Niagara User Parameter specified by TeleportVFXScaleParamName. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport|VFX")
	float TeleportVFXScale = 1.0f;

	/** Niagara User Parameter name to receive TeleportVFXScale (e.g. "Scale"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Teleport|VFX")
	FName TeleportVFXScaleParamName = TEXT("Scale");

protected:
	/** When wait timeout expires, teleport forward instead of dying. */
	virtual void HandleWaitTimeout() override;

	/** Reset stop accumulation after a successful teleport/resume cycle. */
	virtual bool ShouldResetStopTime() const override { return true; }
};
