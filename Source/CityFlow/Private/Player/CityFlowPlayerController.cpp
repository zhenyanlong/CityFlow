#include "Player/CityFlowPlayerController.h"
#include "GameMode/CityFlowGameMode.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "UI/CityFlowHUD.h"
#include "Vehicle/Actor/VehicleActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

#define CF_DEBUG(fmt, ...) if (GEngine) { GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT(fmt), ##__VA_ARGS__)); }

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

	if (bPlacementEnabled)
	{
		SpawnPreview();
	}
}

void ACityFlowPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_PlaceItem)
		{
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Started, this, &ACityFlowPlayerController::OnPlaceItemStarted);
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Triggered, this, &ACityFlowPlayerController::OnPlaceItemTriggered);
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Completed, this, &ACityFlowPlayerController::OnPlaceItemCompleted);
		}

		if (IA_RemoveItem)
		{
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Started, this, &ACityFlowPlayerController::OnRemoveItemStarted);
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Triggered, this, &ACityFlowPlayerController::OnRemoveItemTriggered);
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Completed, this, &ACityFlowPlayerController::OnRemoveItemCompleted);
		}

		if (IA_Pause)
		{
			EnhancedInput->BindAction(IA_Pause, ETriggerEvent::Started, this, &ACityFlowPlayerController::OnPausePressed);
		}
	}
}

void ACityFlowPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPlacementEnabled)
	{
		UpdatePreviewPosition();
	}

	UpdateVehicleHover();
}

void ACityFlowPlayerController::SpawnPreview()
{
	// The preview uses the real placeable class so footprint and doorway validation
	// cannot drift from final placement. Preview actors remain unregistered until the
	// placement transaction succeeds.
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
	// Convert the cursor hit through GridManager and snap once. Visual snapping and
	// CanPlaceAt must receive the same grid coordinate to avoid a green preview that
	// commits into a neighbouring cell.
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

	const bool bCanPlace = PreviewActor->CanPlaceAt(GridPos);
	PreviewActor->SetPreviewPlacementValid(bCanPlace);

	PreviewActor->UpdatePreviewAppearance(GridPos);
}

void ACityFlowPlayerController::DestroyPreview()
{
	if (PreviewActor)
	{
		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

void ACityFlowPlayerController::EnablePlacement()
{
	if (bPlacementEnabled)
	{
		return;
	}

	bPlacementEnabled = true;
	bShowMouseCursor = true;
	SpawnPreview();
}

void ACityFlowPlayerController::DisablePlacement()
{
	if (!bPlacementEnabled)
	{
		return;
	}

	bPlacementEnabled = false;
	DestroyPreview();
}

void ACityFlowPlayerController::TryPlaceAtCursor()
{
	// Drag placement may trigger every input frame. LastPlacedGridPos suppresses
	// duplicate attempts and sounds while still allowing continuous road painting.
	if (!bPlacementEnabled || !PreviewActor || !PlaceableActorClass)
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

	if (GridPos == LastPlacedGridPos)
	{
		return;
	}

	if (GM->GetCellType(GridPos) != ECellType::Empty)
	{
		LastPlacedGridPos = GridPos;
		return;
	}

	PreviewActor->SetActorEnableCollision(true);

	if (PreviewActor->PlaceOnGrid(GridPos))
	{
		PlayPlacementSound();
		LastPlacedGridPos = GridPos;
		PreviewActor = nullptr;
		SpawnPreview();
	}
	else
	{
		PreviewActor->SetActorEnableCollision(false);
	}
}

void ACityFlowPlayerController::PlayPlacementSound()
{
	if (!PlacementSound)
	{
		return;
	}

	UAudioComponent* AudioComponent = UGameplayStatics::CreateSound2D(
		GetWorld(),
		PlacementSound,
		PlacementSoundVolumeMultiplier,
		PlacementSoundPitchMultiplier,
		0.0f,
		nullptr,
		false,
		true);
	if (!AudioComponent)
	{
		return;
	}

	if (PlacementSoundClass)
	{
		AudioComponent->SoundClassOverride = PlacementSoundClass;
	}

	AudioComponent->Play();
}

void ACityFlowPlayerController::OnPlaceItemStarted_Implementation()
{
	LastPlacedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
	TryPlaceAtCursor();
}

void ACityFlowPlayerController::OnPlaceItemTriggered_Implementation()
{
	TryPlaceAtCursor();
}

void ACityFlowPlayerController::OnPlaceItemCompleted_Implementation()
{
	LastPlacedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
}

void ACityFlowPlayerController::TryRemoveAtCursor()
{
	// Removal is restricted to registered road actors. Buildings and transient
	// previews share grid-facing base classes but are not valid right-drag targets.
	if (!bPlacementEnabled)
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

	if (GridPos == LastRemovedGridPos)
	{
		return;
	}
	LastRemovedGridPos = GridPos;

	const FGridCell& Cell = GM->GetCell(GridPos);
	if (Cell.Type == ECellType::Empty)
	{
		return;
	}

	AGridPlaceableActor* HitActor = Cast<AGridPlaceableActor>(Cell.RoadActor);
	if (!HitActor)
	{
		return;
	}

	if (!HitActor->IsPlacedOnGrid())
	{
		return;
	}

	HitActor->RemoveFromGrid();
	HitActor->Destroy();
}

void ACityFlowPlayerController::OnRemoveItemStarted_Implementation()
{
	CF_DEBUG("[Remove] OnRemoveItemStarted_Implementation called!");
	LastRemovedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
	TryRemoveAtCursor();
}

void ACityFlowPlayerController::OnRemoveItemTriggered_Implementation()
{
	TryRemoveAtCursor();
}

void ACityFlowPlayerController::OnRemoveItemCompleted_Implementation()
{
	LastRemovedGridPos = FGridVector(INDEX_NONE, INDEX_NONE);
}

void ACityFlowPlayerController::OnPausePressed_Implementation()
{
	if (ACityFlowHUD* HUD = GetHUD<ACityFlowHUD>())
		HUD->TogglePause();
}

void ACityFlowPlayerController::UpdateVehicleHover()
{
	// Vehicle inspection is available only during Simulation and intentionally uses
	// one hovered actor at a time. Clearing the previous outline before applying the
	// next one prevents custom-depth state from leaking across destroyed vehicles.
	if (!IsSimulationPhaseActive())
	{
		ClearHoveredVehicle();
		return;
	}

	AVehicleActor* HitVehicle = nullptr;
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_GameTraceChannel1, false, Hit))
	{
		HitVehicle = Cast<AVehicleActor>(Hit.GetActor());
	}

	if (!HitVehicle && GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		HitVehicle = Cast<AVehicleActor>(Hit.GetActor());
	}

	if (HoveredVehicle.Get() == HitVehicle)
	{
		return;
	}

	ClearHoveredVehicle();

	if (HitVehicle)
	{
		HoveredVehicle = HitVehicle;
		HitVehicle->SetHovered(true);
	}
}

void ACityFlowPlayerController::ClearHoveredVehicle()
{
	if (AVehicleActor* Vehicle = HoveredVehicle.Get())
	{
		Vehicle->SetHovered(false);
	}
	HoveredVehicle.Reset();
}

bool ACityFlowPlayerController::IsSimulationPhaseActive() const
{
	const ACityFlowGameMode* CityFlowGameMode = Cast<ACityFlowGameMode>(UGameplayStatics::GetGameMode(this));
	return CityFlowGameMode && CityFlowGameMode->GetCurrentPhase() == ECityFlowGamePhase::Simulating;
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
