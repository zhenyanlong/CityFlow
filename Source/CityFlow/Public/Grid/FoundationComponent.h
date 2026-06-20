#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FoundationComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CITYFLOW_API UFoundationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFoundationComponent();

	UFUNCTION(BlueprintCallable, Category = "Foundation")
	void BuildFoundation(float EffWidth, float EffHeight, float CellSize,
		bool bTopConnected, bool bRightConnected, bool bBottomConnected, bool bLeftConnected,
		const FVector& InOwnerScale);

	UFUNCTION(BlueprintCallable, Category = "Foundation")
	void ClearFoundation();

	UFUNCTION()
	static TArray<FString> GetCollisionProfileOptions();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	float FoundationHeight = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	float Padding = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	float CornerRadius = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation", meta = (ClampMin = "1", ClampMax = "32"))
	int32 CornerSegments = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	float SidewalkWidth = 20.0f;

	/** Moves the complete sidewalk ring inward from the generated foundation outline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Sidewalk", meta = (ClampMin = "0.0"))
	float SidewalkInset = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Sidewalk", meta = (ClampMin = "0.0"))
	float SidewalkHeight = 10.0f;

	/** Horizontal amount removed from the top ring on both outer and inner edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Sidewalk", meta = (ClampMin = "0.0"))
	float SidewalkBevelSize = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Sidewalk", meta = (ClampMin = "0.0"))
	float SidewalkBevelHeight = 4.0f;

	/** Embeds the sidewalk slightly into the foundation top to hide the contact seam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Sidewalk", meta = (ClampMin = "0.0"))
	float SidewalkEmbedDepth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Collision")
	bool bCreateFoundationCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Collision")
	bool bCreateSidewalkCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	TObjectPtr<UMaterialInterface> FoundationMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation")
	TObjectPtr<UMaterialInterface> SidewalkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Collision", meta = (GetOptions = "GetCollisionProfileOptions"))
	FName FoundationCollisionProfileName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foundation|Collision", meta = (GetOptions = "GetCollisionProfileOptions"))
	FName SidewalkCollisionProfileName = NAME_None;

private:
	struct FOutlinePoint
	{
		FVector2D Pos;
		bool bIsArc = false;
	};

	struct FEdgeConnection
	{
		bool bTop = false;
		bool bRight = false;
		bool bBottom = false;
		bool bLeft = false;
	};

	void GenerateOutline(const FEdgeConnection& Conn, float HW, float HH,
		TArray<FVector2D>& OutOutline) const;

	void BuildSidewalk(AActor* Owner, const TArray<FVector2D>& FoundationOutline,
		const FVector& InOwnerScale);

	void SanitizeOutline(TArray<FVector2D>& Outline) const;

	bool BuildInsetOutline(const TArray<FVector2D>& Source, float Inset,
		TArray<FVector2D>& OutOutline) const;

	void AddCornerArc(TArray<FVector2D>& OutVerts, FVector2D ArcCenter, float R,
		float StartAngle, float EndAngle, int32 Segments) const;

	float GetEdgePad(float BasePad, bool bConnected) const;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> FoundationMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SidewalkMesh;
};
