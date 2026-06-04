#pragma once

#include "CoreMinimal.h"
#include "CityFlowGridTypes.generated.h"

UENUM(BlueprintType)
enum class ECellType : uint8
{
	Empty    UMETA(DisplayName = "Empty"),
	Road     UMETA(DisplayName = "Road"),
	Building UMETA(DisplayName = "Building")
};

UENUM(BlueprintType)
enum class EPlaceableType : uint8
{
	Road      UMETA(DisplayName = "Road"),
	Building  UMETA(DisplayName = "Building"),
	Landscape UMETA(DisplayName = "Landscape")
};

UENUM(BlueprintType)
enum class EGridRotation : uint8
{
	Rot0   UMETA(DisplayName = "0°"),
	Rot90  UMETA(DisplayName = "90°"),
	Rot180 UMETA(DisplayName = "180°"),
	Rot270 UMETA(DisplayName = "270°")
};

UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGridDirection : uint8
{
	None  = 0       UMETA(Hidden),
	Up    = 1 << 0  UMETA(DisplayName = "Up"),
	Down  = 1 << 1  UMETA(DisplayName = "Down"),
	Left  = 1 << 2  UMETA(DisplayName = "Left"),
	Right = 1 << 3  UMETA(DisplayName = "Right")
};
ENUM_CLASS_FLAGS(EGridDirection)

USTRUCT(BlueprintType)
struct CITYFLOW_API FGridCell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ECellType Type = ECellType::Empty;

	UPROPERTY(BlueprintReadOnly)
	uint8 ConnectedMask = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 BuildingID = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<AActor> RoadActor = nullptr;
};

USTRUCT(BlueprintType)
struct CITYFLOW_API FGridVector
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Y = 0;

	FGridVector() = default;
	FGridVector(int32 InX, int32 InY) : X(InX), Y(InY) {}

	bool operator==(const FGridVector& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const FGridVector& Other) const
	{
		return !(*this == Other);
	}

	FGridVector operator+(const FGridVector& Other) const
	{
		return FGridVector(X + Other.X, Y + Other.Y);
	}

	FGridVector operator-(const FGridVector& Other) const
	{
		return FGridVector(X - Other.X, Y - Other.Y);
	}

	friend uint32 GetTypeHash(const FGridVector& Vec)
	{
		return HashCombine(GetTypeHash(Vec.X), GetTypeHash(Vec.Y));
	}

	bool IsZero() const
	{
		return X == 0 && Y == 0;
	}
};

namespace GridDirectionUtils
{
	static const FGridVector Up(0, -1);
	static const FGridVector Down(0, 1);
	static const FGridVector Left(-1, 0);
	static const FGridVector Right(1, 0);

	static FGridVector GetVector(EGridDirection Dir)
	{
		switch (Dir)
		{
		case EGridDirection::Up:    return Up;
		case EGridDirection::Down:  return Down;
		case EGridDirection::Left:  return Left;
		case EGridDirection::Right: return Right;
		default:                    return FGridVector();
		}
	}

	static EGridDirection GetOpposite(EGridDirection Dir)
	{
		switch (Dir)
		{
		case EGridDirection::Up:    return EGridDirection::Down;
		case EGridDirection::Down:  return EGridDirection::Up;
		case EGridDirection::Left:  return EGridDirection::Right;
		case EGridDirection::Right: return EGridDirection::Left;
		default:                    return EGridDirection::None;
		}
	}

	static TArray<EGridDirection> GetAllDirections()
	{
		return { EGridDirection::Up, EGridDirection::Down, EGridDirection::Left, EGridDirection::Right };
	}

	static EGridDirection Rotate90CW(EGridDirection Dir)
	{
		switch (Dir)
		{
		case EGridDirection::Up:    return EGridDirection::Right;
		case EGridDirection::Right: return EGridDirection::Down;
		case EGridDirection::Down:  return EGridDirection::Left;
		case EGridDirection::Left:  return EGridDirection::Up;
		default:                    return EGridDirection::None;
		}
	}

	/** Determine dominant grid direction from a world direction vector (X/Y only, Z ignored). */
	static EGridDirection DirectionFromWorldVector(const FVector& WorldDir)
	{
		if (WorldDir.IsNearlyZero())
		{
			return EGridDirection::None;
		}

		const float AbsX = FMath::Abs(WorldDir.X);
		const float AbsY = FMath::Abs(WorldDir.Y);

		if (AbsX >= AbsY)
		{
			return WorldDir.X > 0.0f ? EGridDirection::Right : EGridDirection::Left;
		}
		else
		{
			return WorldDir.Y > 0.0f ? EGridDirection::Down : EGridDirection::Up;
		}
	}

	/** Determine grid direction from a grid delta (must be axis-aligned). */
	static EGridDirection DirectionFromGridDelta(const FGridVector& Delta)
	{
		if (Delta.X > 0) return EGridDirection::Right;
		if (Delta.X < 0) return EGridDirection::Left;
		if (Delta.Y > 0) return EGridDirection::Down;
		if (Delta.Y < 0) return EGridDirection::Up;
		return EGridDirection::None;
	}

	/** Returns true if the two grid directions are perpendicular (e.g. Up and Left). */
	static bool ArePerpendicular(EGridDirection A, EGridDirection B)
	{
		if (A == EGridDirection::None || B == EGridDirection::None)
		{
			return false;
		}
		return A != B && A != GetOpposite(B);
	}
}
