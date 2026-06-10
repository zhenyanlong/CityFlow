#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Grid/MeshGridPlaceableActor.h"
#include "RoadTile.generated.h"

USTRUCT(BlueprintType)
struct CITYFLOW_API FRoadMeshConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	uint8 CanonicalMask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	FVector ScaleMultiplier = FVector(1.0f, 1.0f, 1.0f);
};

UCLASS()
class CITYFLOW_API ARoadTile : public AMeshGridPlaceableActor
{
	GENERATED_BODY()

public:
	ARoadTile();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	TArray<FRoadMeshConfig> RoadMeshConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road")
	float ReferenceCellSize = 200.0f;

	/** Trigger box for intersection collision — enabled for Cross and T-junction tiles. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road|Intersection")
	TObjectPtr<UBoxComponent> IntersectionBox;

	/** Vertical half-extent of the intersection box (Z axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection")
	float IntersectionBoxHalfHeight = 200.0f;

	// ---- Intersection occupancy indicator ----

	/** Plane mesh floating above the intersection to indicate occupancy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Road|Intersection|Indicator")
	TObjectPtr<UStaticMeshComponent> IndicatorPlane;

	/** Material for the indicator plane. Must expose a VectorParameter named "Color" (emissive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection|Indicator")
	TObjectPtr<UMaterialInterface> IndicatorMaterial;

	/** Size of the indicator relative to cell size (0.0~1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection|Indicator")
	float IndicatorSize = 0.4f;

	/** Z offset above the intersection box top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection|Indicator")
	float IndicatorZOffset = 80.0f;

	/** Color when the intersection is free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection|Indicator")
	FLinearColor IndicatorFreeColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

	/** Color when the intersection is occupied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road|Intersection|Indicator")
	FLinearColor IndicatorOccupiedColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Road")
	void UpdateAppearance();

	UFUNCTION(BlueprintCallable, Category = "Road|Spline")
	bool GetSplinePath(EGridDirection EntryDir, EGridDirection ExitDir, TArray<FVector>& OutWorldPoints) const;

	virtual void UpdatePreviewAppearance(const FGridVector& GridPos) override;

	virtual void OnEnterPlaced_Implementation() override;
	virtual void OnPreviewValidChanged_Implementation(bool bValid) override;

	// ---- Intersection lock API (called by VehicleActor forward probe) ----

	/**
	 * Attempt to reserve this intersection for the given entry direction.
	 * - Same direction as existing occupants → allowed (follow-through).
	 * - Crossing direction → rejected (return false).
	 * Returns true if reservation is granted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Road|Intersection")
	bool TryAcquireIntersectionLock(class AVehicleActor* Vehicle, EGridDirection EntryDir);

	/** Returns true if this tile is a road intersection (ConnectedMask has >= 3 bits set). */
	UFUNCTION(BlueprintPure, Category = "Road|Intersection")
	bool IsIntersection() const;

	/** Returns true if any direction is currently occupied. */
	UFUNCTION(BlueprintPure, Category = "Road|Intersection")
	bool IsAnyDirectionOccupied() const;

	/** Remove a vehicle from all direction-occupancy tables. Safe to call from VehicleActor destruction/arrival. */
	void ReleaseVehicleFromAllTables(class AVehicleActor* Vehicle);

	/**
	 * Safety-net: check every vehicle in DirectionOccupants via IsOverlappingActor.
	 * Removes entries whose vehicle no longer physically overlaps the box
	 * (catches lost EndOverlap events).
	 */
	void SanitizeOccupants();

	/**
	 * Expire pending reservations older than MaxAgeSeconds.
	 * Prevents vehicles stuck in traffic from permanently blocking their entry direction.
	 */
	void ExpirePendingReservations(float MaxAgeSeconds);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPlacedOnGrid_Implementation() override;
	virtual void OnRemovedFromGrid_Implementation() override;
	virtual ECellType GetPlacementCellType() const override { return ECellType::Road; }

private:
	UFUNCTION()
	void OnGridCellChanged(FGridVector CellPos, const FGridCell& NewCell);

	UFUNCTION()
	void OnIntersectionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnIntersectionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdateIntersectionBox();
	void UpdateIndicator();
	void UpdateIndicatorState();

	EGridDirection FindEntryDirForVehicle(class AVehicleActor* Vehicle) const;

	bool FindMeshConfig(uint8 Mask, UStaticMesh*& OutMesh, float& OutYaw, FVector& OutScaleMultiplier) const;
	static uint8 RotateMask90CW(uint8 Mask);

	void EnsureMeshMaterialsCached(UStaticMesh* Mesh);
	void RestoreMeshMaterials(UStaticMesh* Mesh);

	TMap<TObjectPtr<UStaticMesh>, TArray<TObjectPtr<UMaterialInterface>>> MeshMaterialCache;

	/**
	 * Vehicles currently physically inside the intersection box, grouped by their entry direction.
	 * Key = entry direction (the grid-facing direction they entered from).
	 * Value = set of vehicles that entered from this direction and are still inside the box.
	 */
	TMap<EGridDirection, TSet<TWeakObjectPtr<class AVehicleActor>>> DirectionOccupants;

	/**
	 * Vehicles that have been granted a reservation via TryAcquireIntersectionLock
	 * but have not yet physically entered the box (still on approach).
	 * Key = entry direction.
	 */
	TMap<EGridDirection, TSet<TWeakObjectPtr<class AVehicleActor>>> PendingReservations;

	// ---- Indicator ----

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> IndicatorDMI;

	/**
	 * Reverse-lookup: for a given vehicle, which entry direction was it granted/last recorded for.
	 * Used during EndOverlap to clean up the correct direction table.
	 */
	TMap<TWeakObjectPtr<class AVehicleActor>, EGridDirection> VehicleEntryDirs;

	/** Timestamps for pending reservations (world time when granted). */
	TMap<TWeakObjectPtr<class AVehicleActor>, float> PendingReservationTimestamps;

	// ---- Round-robin direction scheduling ----

	/** Direction currently being served when multiple directions compete. */
	EGridDirection ServingDirection = EGridDirection::None;

	/** How many vehicles have been granted for the current ServingDirection this round. */
	int32 ServedCount = 0;

	/** Directions that are waiting for their turn (losing the competition). */
	TSet<EGridDirection> WaitingDirs;

	/** Max vehicles to serve per direction per round before switching (default 1). */
	static constexpr int32 MaxConsecutiveGrants = 1;
};
