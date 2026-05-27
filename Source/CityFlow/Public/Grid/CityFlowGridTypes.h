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

	UPROPERTY(BlueprintReadWrite)
	int32 X = 0;

	UPROPERTY(BlueprintReadWrite)
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
}
