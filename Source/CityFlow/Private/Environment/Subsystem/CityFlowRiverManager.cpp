#include "Environment/Subsystem/CityFlowRiverManager.h"

#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Environment/CityFlowRiverSettings.h"
#include "EngineUtils.h"
#include "Grid/GridManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"

void UCityFlowRiverManager::Deinitialize()
{
	ClearRivers();
	Super::Deinitialize();
}

void UCityFlowRiverManager::GenerateRivers(int32 Seed)
{
	UGridManager* GM = GetGridManager();
	if (!GM || !GM->IsGridInitialized())
	{
		return;
	}

	const UCityFlowRiverSettings* Settings = GetDefault<UCityFlowRiverSettings>();
	if (!Settings || !Settings->bAutoGenerateOnNewGame || Settings->RiverCount <= 0)
	{
		ClearRivers();
		return;
	}

	ClearRivers();

	const int32 RTSize = FMath::Max(128, Settings->RenderTargetSize);
	RiverMaskRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CityFlowRiverMaskRT"));
	RiverMaskRenderTarget->RenderTargetFormat = RTF_RGBA8;
	RiverMaskRenderTarget->ClearColor = Settings->ClearColor;
	RiverMaskRenderTarget->InitAutoFormat(RTSize, RTSize);
	RiverMaskRenderTarget->UpdateResourceImmediate(true);

	const int32 EffectiveSeed = (Seed == -1) ? Settings->RandomSeed : Seed;
	FRandomStream RandomStream(EffectiveSeed);
	const int32 RiverCount = FMath::Clamp(Settings->RiverCount, 0, 4);

	for (int32 i = 0; i < RiverCount; ++i)
	{
		FCityFlowRiverPath RiverPath = GenerateSingleRiver(RandomStream);
		RasterizeRiverPath(RiverPath);
		RiverPaths.Add(MoveTemp(RiverPath));
	}

	DrawRiverMask();
	ApplyRiverMaskToLandscapeMaterial();
}

void UCityFlowRiverManager::ClearRivers()
{
	SetLandscapeRiverMaskEnabled(0.0f);
	RiverPaths.Empty();
	RiverCells.Empty();
	RiverBankCells.Empty();
	RiverMaskRenderTarget = nullptr;
}

bool UCityFlowRiverManager::IsRiverCell(FGridVector CellPos) const
{
	return RiverCells.Contains(CellPos);
}

bool UCityFlowRiverManager::IsRiverOrBankCell(FGridVector CellPos) const
{
	return RiverCells.Contains(CellPos) || RiverBankCells.Contains(CellPos);
}

void UCityFlowRiverManager::ApplyRiverMaskToLandscapeMaterial()
{
	UGridManager* GM = GetGridManager();
	UWorld* World = GetWorld();
	if (!GM || !GM->IsGridInitialized() || !World || !RiverMaskRenderTarget)
	{
		SetLandscapeRiverMaskEnabled(0.0f);
		return;
	}

	const float CellSize = GM->GetCellSize();
	const FVector Origin = GM->GetGridOrigin();
	const float GridMinX = Origin.X - CellSize * 0.5f;
	const float GridMinY = Origin.Y - CellSize * 0.5f;
	const float GridWorldSizeX = GM->GetGridWidth() * CellSize;
	const float GridWorldSizeY = GM->GetGridHeight() * CellSize;

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* LandscapeProxy = *It;
		if (!LandscapeProxy)
		{
			continue;
		}

		LandscapeProxy->SetLandscapeMaterialTextureParameterValue(TEXT("RiverMaskTexture"), RiverMaskRenderTarget);
		LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("RiverMaskEnabled"), 1.0f);
		LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("GridMinX"), GridMinX);
		LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("GridMinY"), GridMinY);
		LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("GridWorldSizeX"), GridWorldSizeX);
		LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("GridWorldSizeY"), GridWorldSizeY);
	}
}

FCityFlowRiverPath UCityFlowRiverManager::GenerateSingleRiver(FRandomStream& RandomStream) const
{
	FCityFlowRiverPath RiverPath;

	UGridManager* GM = GetGridManager();
	const UCityFlowRiverSettings* Settings = GetDefault<UCityFlowRiverSettings>();
	if (!GM || !Settings)
	{
		return RiverPath;
	}

	const int32 StartEdge = RandomStream.RandRange(0, 3);
	int32 EndEdge = RandomStream.RandRange(0, 3);
	if (EndEdge == StartEdge)
	{
		EndEdge = (StartEdge + 2) % 4;
	}

	const FVector Start = PickEdgePoint(StartEdge, RandomStream);
	const FVector End = PickEdgePoint(EndEdge, RandomStream);
	const FVector MainDir = (End - Start).GetSafeNormal2D();
	const FVector Perp(-MainDir.Y, MainDir.X, 0.0f);

	const int32 SegmentCount = FMath::Max(2, Settings->SegmentCount);
	const float CellSize = GM->GetCellSize();
	const float Sinuosity = FMath::Max(0.0f, Settings->SinuosityCells) * CellSize;

	RiverPath.Points.Reserve(SegmentCount + 1);
	for (int32 i = 0; i <= SegmentCount; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(SegmentCount);
		FVector Point = FMath::Lerp(Start, End, T);

		const float EdgeFade = FMath::Sin(T * PI);
		const float Offset = RandomStream.FRandRange(-Sinuosity, Sinuosity) * EdgeFade;
		Point += Perp * Offset;
		Point.Z = GM->GetGridOrigin().Z;
		RiverPath.Points.Add(Point);
	}

	return RiverPath;
}

void UCityFlowRiverManager::RasterizeRiverPath(FCityFlowRiverPath& RiverPath)
{
	UGridManager* GM = GetGridManager();
	const UCityFlowRiverSettings* Settings = GetDefault<UCityFlowRiverSettings>();
	if (!GM || !Settings || RiverPath.Points.Num() < 2)
	{
		return;
	}

	const float CellSize = GM->GetCellSize();
	const float RiverRadius = FMath::Max(0.05f, Settings->RiverWidthCells * 0.5f) * CellSize;
	const float BankRadius = RiverRadius + FMath::Max(0.0f, Settings->BankWidthCells) * CellSize;

	for (int32 Y = 0; Y < GM->GetGridHeight(); ++Y)
	{
		for (int32 X = 0; X < GM->GetGridWidth(); ++X)
		{
			const FGridVector CellPos(X, Y);
			const FVector CellWorld = GM->GridToWorld(CellPos);
			const FVector2D P(CellWorld.X, CellWorld.Y);

			float MinDist = TNumericLimits<float>::Max();
			for (int32 i = 0; i < RiverPath.Points.Num() - 1; ++i)
			{
				const FVector2D A(RiverPath.Points[i].X, RiverPath.Points[i].Y);
				const FVector2D B(RiverPath.Points[i + 1].X, RiverPath.Points[i + 1].Y);
				MinDist = FMath::Min(MinDist, DistancePointToSegment2D(P, A, B));
			}

			if (MinDist <= RiverRadius)
			{
				RiverPath.RiverCells.Add(CellPos);
				RiverCells.Add(CellPos);
			}
			else if (MinDist <= BankRadius)
			{
				RiverPath.BankCells.Add(CellPos);
				RiverBankCells.Add(CellPos);
			}
		}
	}
}

void UCityFlowRiverManager::DrawRiverMask()
{
	UWorld* World = GetWorld();
	const UCityFlowRiverSettings* Settings = GetDefault<UCityFlowRiverSettings>();
	UGridManager* GM = GetGridManager();
	if (!World || !Settings || !GM || !RiverMaskRenderTarget)
	{
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, RiverMaskRenderTarget, Settings->ClearColor);

	UCanvas* Canvas = nullptr;
	FVector2D MaskSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RiverMaskRenderTarget, Canvas, MaskSize, Context);

	if (!Canvas)
	{
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
		return;
	}

	const float CellSize = GM->GetCellSize();
	const float WorldSize = GM->GetGridWidth() * CellSize;
	const float PixelPerWorldUnit = MaskSize.X / FMath::Max(1.0f, WorldSize);
	const float RiverThickness = FMath::Max(1.0f, Settings->RiverWidthCells * CellSize * PixelPerWorldUnit);
	const float BankThickness = FMath::Max(RiverThickness, (Settings->RiverWidthCells + Settings->BankWidthCells * 2.0f) * CellSize * PixelPerWorldUnit);

	for (const FCityFlowRiverPath& RiverPath : RiverPaths)
	{
		for (int32 i = 0; i < RiverPath.Points.Num() - 1; ++i)
		{
			const FVector2D A = WorldToMaskPixel(RiverPath.Points[i], MaskSize);
			const FVector2D B = WorldToMaskPixel(RiverPath.Points[i + 1], MaskSize);
			Canvas->K2_DrawLine(A, B, BankThickness, Settings->BankMaskColor);
		}
	}

	for (const FCityFlowRiverPath& RiverPath : RiverPaths)
	{
		for (int32 i = 0; i < RiverPath.Points.Num() - 1; ++i)
		{
			const FVector2D A = WorldToMaskPixel(RiverPath.Points[i], MaskSize);
			const FVector2D B = WorldToMaskPixel(RiverPath.Points[i + 1], MaskSize);
			Canvas->K2_DrawLine(A, B, RiverThickness, Settings->WaterMaskColor);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
}

void UCityFlowRiverManager::SetLandscapeRiverMaskEnabled(float EnabledValue)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		ALandscapeProxy* LandscapeProxy = *It;
		if (LandscapeProxy)
		{
			LandscapeProxy->SetLandscapeMaterialScalarParameterValue(TEXT("RiverMaskEnabled"), EnabledValue);
		}
	}
}

FVector2D UCityFlowRiverManager::WorldToMaskPixel(const FVector& WorldPosition, FVector2D MaskSize) const
{
	const UGridManager* GM = GetGridManager();
	if (!GM)
	{
		return FVector2D::ZeroVector;
	}

	const float CellSize = GM->GetCellSize();
	const FVector Origin = GM->GetGridOrigin();
	const float MinX = Origin.X - CellSize * 0.5f;
	const float MinY = Origin.Y - CellSize * 0.5f;
	const float WorldWidth = GM->GetGridWidth() * CellSize;
	const float WorldHeight = GM->GetGridHeight() * CellSize;

	const float U = (WorldPosition.X - MinX) / FMath::Max(1.0f, WorldWidth);
	const float V = (WorldPosition.Y - MinY) / FMath::Max(1.0f, WorldHeight);
	return FVector2D(U * MaskSize.X, V * MaskSize.Y);
}

float UCityFlowRiverManager::DistancePointToSegment2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B) const
{
	const FVector2D AB = B - A;
	const float LenSq = AB.SizeSquared();
	if (LenSq <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::Distance(Point, A);
	}

	const float T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / LenSq, 0.0f, 1.0f);
	const FVector2D Closest = A + AB * T;
	return FVector2D::Distance(Point, Closest);
}

FVector UCityFlowRiverManager::PickEdgePoint(int32 Edge, FRandomStream& RandomStream) const
{
	const UGridManager* GM = GetGridManager();
	const UCityFlowRiverSettings* Settings = GetDefault<UCityFlowRiverSettings>();
	if (!GM || !Settings)
	{
		return FVector::ZeroVector;
	}

	const int32 Margin = FMath::Max(0, Settings->EdgeMarginCells);
	const int32 MinX = FMath::Clamp(Margin, 0, GM->GetGridWidth() - 1);
	const int32 MaxX = FMath::Clamp(GM->GetGridWidth() - 1 - Margin, 0, GM->GetGridWidth() - 1);
	const int32 MinY = FMath::Clamp(Margin, 0, GM->GetGridHeight() - 1);
	const int32 MaxY = FMath::Clamp(GM->GetGridHeight() - 1 - Margin, 0, GM->GetGridHeight() - 1);

	FGridVector CellPos;
	switch (Edge)
	{
	case 0:
		CellPos = FGridVector(RandomStream.RandRange(MinX, MaxX), 0);
		break;
	case 1:
		CellPos = FGridVector(GM->GetGridWidth() - 1, RandomStream.RandRange(MinY, MaxY));
		break;
	case 2:
		CellPos = FGridVector(RandomStream.RandRange(MinX, MaxX), GM->GetGridHeight() - 1);
		break;
	default:
		CellPos = FGridVector(0, RandomStream.RandRange(MinY, MaxY));
		break;
	}

	return GM->GridToWorld(CellPos);
}

UGridManager* UCityFlowRiverManager::GetGridManager() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<UGridManager>();
}
