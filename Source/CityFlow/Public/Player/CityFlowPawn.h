#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CityFlowPawn.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class CITYFLOW_API ACityFlowPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ACityFlowPawn();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetMainMenuCameraYawRotationEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ResetToInitialViewState(bool bResetLocation);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void StopCameraMovement();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void Zoom(const struct FInputActionValue& Value);
	void OnAltPressed();
	void OnAltReleased();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Zoom;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Alt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 1500.0f;

	/** Current facing yaw (set by Blueprint from camera orientation). WASD movement is computed from this yaw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraYaw = 0.0f;

	/** Target spring arm length, driven by scroll wheel. Blueprint reads this to adjust the spring arm component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float TargetSpringArmLength = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float LookSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Main Menu")
	float MainMenuCameraYawSpeed = 4.0f;

	/** Scroll wheel zoom speed multiplier. Blueprint can tweak this at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinSpringArmLength = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxSpringArmLength = 20000.0f;

	/** Initial camera pitch on game start (negative = looking down). Top-down default: -60°. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float DefaultCameraPitch = -60.0f;

	/** Minimum camera pitch (negative = more top-down, e.g. -80°). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinCameraPitch = -80.0f;

	/** Maximum camera pitch (negative = less steep, e.g. -30°). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxCameraPitch = -30.0f;

private:
	FTransform InitialPawnTransform;
	FRotator InitialControlRotation = FRotator::ZeroRotator;
	bool bAltHeld = false;
	bool bMainMenuCameraYawRotationEnabled = false;
};
