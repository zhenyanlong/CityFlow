#include "Player/CityFlowPlayerController.h"
#include "Grid/GridManager.h"
#include "Grid/GridPlaceableActor.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

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
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Triggered, this, &ACityFlowPlayerController::OnPlaceItemTriggered);
			EnhancedInput->BindAction(IA_PlaceItem, ETriggerEvent::Completed, this, &ACityFlowPlayerController::OnPlaceItemCompleted);
		}

		if (IA_RemoveItem)
		{
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Started, this, &ACityFlowPlayerController::OnRemoveItemStarted);
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Triggered, this, &ACityFlowPlayerController::OnRemoveItemTriggered);
			EnhancedInput->BindAction(IA_RemoveItem, ETriggerEvent::Completed, this, &ACityFlowPlayerController::OnRemoveItemCompleted);
			CF_DEBUG("[Remove] IA_RemoveItem bound to OnRemoveItemStarted");
		}
		else
		{
			CF_DEBUG("[Remove] IA_RemoveItem is NULL - not configured in Blueprint!");
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

void ACityFlowPlayerController::TryPlaceAtCursor()
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
	PreviewActor->PlaceOnGrid(GridPos);
	LastPlacedGridPos = GridPos;
	PreviewActor = nullptr;

	SpawnPreview();
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

UGridManager* ACityFlowPlayerController::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
