#include "Player/CityFlowPawn.h"
#include "Player/CityFlowPlayerController.h"
#include "GameMode/CityFlowGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ACityFlowPawn::ACityFlowPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* CharMovement = GetCharacterMovement();
	CharMovement->bOrientRotationToMovement = false;
	CharMovement->RotationRate = FRotator(0.0f);
	CharMovement->MovementMode = MOVE_Flying;
	CharMovement->BrakingDecelerationFlying = MoveSpeed * 4.0f;
	CharMovement->MaxFlySpeed = MoveSpeed;
}

void ACityFlowPawn::SetMainMenuCameraYawRotationEnabled(bool bEnabled)
{
	bMainMenuCameraYawRotationEnabled = bEnabled;
}

void ACityFlowPawn::ResetToInitialViewState(bool bResetLocation)
{
	StopCameraMovement();

	if (bResetLocation)
	{
		SetActorTransform(InitialPawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(InitialControlRotation);
		CameraYaw = InitialControlRotation.Yaw;
	}
}

void ACityFlowPawn::StopCameraMovement()
{
	bAltHeld = false;

	if (UCharacterMovementComponent* CharMovement = GetCharacterMovement())
	{
		CharMovement->StopMovementImmediately();
	}
}

void ACityFlowPawn::BeginPlay()
{
	Super::BeginPlay();

	InitialPawnTransform = GetActorTransform();

	UCharacterMovementComponent* CharMovement = GetCharacterMovement();
	CharMovement->MaxFlySpeed = MoveSpeed;
	CharMovement->BrakingDecelerationFlying = MoveSpeed * 4.0f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator InitialRot = PC->GetControlRotation();
		InitialRot.Pitch = DefaultCameraPitch;
		PC->SetControlRotation(InitialRot);
		InitialControlRotation = InitialRot;
		CameraYaw = InitialRot.Yaw;

		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DefaultMappingContext)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
			}
		}
	}
}

void ACityFlowPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMainMenuCameraYawRotationEnabled || FMath::IsNearlyZero(MainMenuCameraYawSpeed))
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		FRotator CameraRotation = PC->GetControlRotation();
		CameraRotation.Yaw += MainMenuCameraYawSpeed * DeltaSeconds;
		PC->SetControlRotation(CameraRotation);
		CameraYaw = CameraRotation.Yaw;
	}
}

void ACityFlowPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ACityFlowPawn::Move);
		}

		if (IA_Look)
		{
			EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ACityFlowPawn::Look);
		}

		if (IA_Zoom)
		{
			EnhancedInput->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &ACityFlowPawn::Zoom);
		}

		if (IA_Alt)
		{
			EnhancedInput->BindAction(IA_Alt, ETriggerEvent::Started, this, &ACityFlowPawn::OnAltPressed);
			EnhancedInput->BindAction(IA_Alt, ETriggerEvent::Completed, this, &ACityFlowPawn::OnAltReleased);
		}
	}
}

void ACityFlowPawn::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (Input.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.0f, CameraYaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetScaledAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetScaledAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ACityFlowPawn::Look(const FInputActionValue& Value)
{
	if (!bAltHeld)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	if (!FMath::IsNearlyZero(Input.X))
	{
		AddControllerYawInput(Input.X * LookSensitivity);
	}
	// Pitch control handled in Blueprint.
}

void ACityFlowPawn::Zoom(const FInputActionValue& Value)
{
	const float WheelDelta = Value.Get<float>();
	TargetSpringArmLength = FMath::Clamp(
		TargetSpringArmLength - WheelDelta * ZoomSpeed,
		MinSpringArmLength,
		MaxSpringArmLength);
}

void ACityFlowPawn::OnAltPressed()
{
	bAltHeld = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());

		if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
			CFPC->DisablePlacement();
	}
}

void ACityFlowPawn::OnAltReleased()
{
	bAltHeld = false;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);

		// Restore placement only in Planning; enabling it in Simulation would allow
		// the player to mutate the graph while vehicles are following cached paths.
		if (ACityFlowGameMode* GM = Cast<ACityFlowGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (GM->GetCurrentPhase() == ECityFlowGamePhase::Planning)
			{
				if (ACityFlowPlayerController* CFPC = Cast<ACityFlowPlayerController>(PC))
					CFPC->EnablePlacement();
			}
		}
	}
}
