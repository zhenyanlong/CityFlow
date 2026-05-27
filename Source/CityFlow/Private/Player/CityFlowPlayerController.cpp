#include "Player/CityFlowPlayerController.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"

ACityFlowPlayerController::ACityFlowPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ACityFlowPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	SpawnPreview();
}

void ACityFlowPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_PlaceItem)
		{
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Started, this, &ACityFlowPlayerController::OnPlaceItemStarted);
		}
	}
}

void ACityFlowPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePreviewPosition();
}

void ACityFlowPlayerController::SpawnPreview()
{
	if (!PlaceableActorClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PreviewActor = GetWorld()->SpawnActor<AGridPlaceableActor>(PlaceableActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (PreviewActor)
	{
		PreviewActor->SetActorEnableCollision(false);
		PreviewActor->SetActorHiddenInGame(false);
		PreviewActor->EnterPreviewState();
	}
}

void ACityFlowPlayerController::UpdatePreviewPosition()
{
	if (!PreviewActor)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}

	const FGridVector GridPos = GM->WorldToGrid(Hit.Location);
	if (!GM->IsValidGridPos(GridPos))
	{
		return;
	}

	const FVector SnappedPos = GM->GridToWorld(GridPos);
	PreviewActor->SetActorLocation(SnappedPos);
}

void ACityFlowPlayerController::DestroyPreview()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

void ACityFlowPlayerController::OnPlaceItemStarted_Implementation()
{
	if (!PreviewActor || !PlaceableActorClass)
	{
		return;
	}

	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}

	const FVector HitLocation = Hit.Location;
	const FGridVector GridPos = GM->WorldToGrid(HitLocation);

	if (!GM->IsValidGridPos(GridPos))
	{
		return;
	}

	if (GM->GetCellType(GridPos) != ECellType::Empty)
	{
		return;
	}

	PreviewActor->SetActorEnableCollision(true);
	PreviewActor->PlaceOnGrid(GridPos);
	PreviewActor = nullptr;

	SpawnPreview();
}

UGridManager* ACityFlowPlayerController::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
