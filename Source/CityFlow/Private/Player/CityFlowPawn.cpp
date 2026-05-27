#include "Player/CityFlowPawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ACityFlowPawn::ACityFlowPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* CharMovement = GetCharacterMovement();
	CharMovement->bOrientRotationToMovement = false;
	CharMovement->RotationRate = FRotator(0.0f);
	CharMovement->MovementMode = MOVE_Flying;
	CharMovement->BrakingDecelerationFlying = MoveSpeed * 4.0f;
	CharMovement->MaxFlySpeed = MoveSpeed;
}

void ACityFlowPawn::BeginPlay()
{
	Super::BeginPlay();

	UCharacterMovementComponent* CharMovement = GetCharacterMovement();
	CharMovement->MaxFlySpeed = MoveSpeed;
	CharMovement->BrakingDecelerationFlying = MoveSpeed * 4.0f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
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

void ACityFlowPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ACityFlowPawn::Move);
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

	const FRotator CameraRotation = GetControlRotation();
	const FVector Forward = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::X);
	const FVector Right = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}
