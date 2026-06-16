#include "Grid/FoundationComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"

UFoundationComponent::UFoundationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

TArray<FString> UFoundationComponent::GetCollisionProfileOptions()
{
    TArray<FString> Options;
    Options.Add(TEXT(""));

    UCollisionProfile* ProfileManager = UCollisionProfile::Get();
    if (!ProfileManager)
    {
        return Options;
    }

    int32 NumProfiles = ProfileManager->GetNumOfProfiles();
    for (int32 i = 0; i < NumProfiles; ++i)
    {
        const FCollisionResponseTemplate* Profile = ProfileManager->GetProfileByIndex(i);
        if (Profile)
        {
            Options.Add(Profile->Name.ToString());
        }
    }

    return Options;
}

void UFoundationComponent::BuildFoundation(float EffWidth, float EffHeight, float CellSize,
    bool bTopConnected, bool bRightConnected, bool bBottomConnected, bool bLeftConnected,
    const FVector& InOwnerScale)
{
    ClearFoundation();

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const float HW = EffWidth * CellSize * 0.5f;
    const float HH = EffHeight * CellSize * 0.5f;

    const FEdgeConnection Conn = { bTopConnected, bRightConnected, bBottomConnected, bLeftConnected };

    TArray<FVector2D> Outline;
    GenerateOutline(Conn, HW, HH, Outline);

    const int32 N = Outline.Num();
    if (N < 3)
    {
        return;
    }

    const float TopZ = FoundationHeight;
    const float BotZ = 0.0f;

    // ================= 1. 顶面数据 =================
    TArray<FVector> TopVerts, TopNormals;
    TArray<int32> TopTris;
    TArray<FVector2D> TopUVs;

    TopVerts.Reserve(N);
    TopNormals.Reserve(N);
    TopUVs.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        TopVerts.Add(FVector(Outline[i].X, Outline[i].Y, TopZ));
        TopNormals.Add(FVector::UpVector);
        TopUVs.Add(FVector2D(Outline[i].X * 0.01f, Outline[i].Y * 0.01f));
    }

    // 修正：外轮廓是逆时针，顶面法线朝上，需要调换 i+1 和 i+2 变为顺时针
    for (int32 i = 0; i < N - 2; ++i)
    {
        TopTris.Add(0);
        TopTris.Add(i + 2);
        TopTris.Add(i + 1);
    }

    // ================= 2. 底面数据 =================
    TArray<FVector> BotVerts, BotNormals;
    TArray<int32> BotTris;
    TArray<FVector2D> BotUVs;

    BotVerts.Reserve(N);
    BotNormals.Reserve(N);
    BotUVs.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        BotVerts.Add(FVector(Outline[i].X, Outline[i].Y, BotZ));
        BotNormals.Add(FVector::DownVector);
        BotUVs.Add(FVector2D(Outline[i].X * 0.01f, Outline[i].Y * 0.01f));
    }

    // 修正：底面法线朝下。从下面往上看，原来的逆时针刚好变成了顺时针，所以保持 0, i+1, i+2
    for (int32 i = 0; i < N - 2; ++i)
    {
        BotTris.Add(0);
        BotTris.Add(i + 1);
        BotTris.Add(i + 2);
    }

    // ================= 3. 侧墙数据 =================
    TArray<FVector> WallVerts;
    TArray<int32> WallTris;
    TArray<FVector> WallNormals;
    TArray<FVector2D> WallUVs;

    float UVOffset = 0.0f;
    for (int32 i = 0; i < N; ++i)
    {
        const int32 Next = (i + 1) % N;

        const FVector2D& A = Outline[i];
        const FVector2D& B = Outline[Next];

        const FVector2D Edge = B - A;
        const float EdgeLen = Edge.Size();
        FVector2D EdgeNormal(Edge.Y, -Edge.X);
        EdgeNormal.Normalize();
        const FVector OutNormal(EdgeNormal.X, EdgeNormal.Y, 0.0f);

        const int32 Base = WallVerts.Num();
        WallVerts.Add(FVector(A.X, A.Y, TopZ));
        WallVerts.Add(FVector(B.X, B.Y, TopZ));
        WallVerts.Add(FVector(B.X, B.Y, BotZ));
        WallVerts.Add(FVector(A.X, A.Y, BotZ));

        // 修正：从外面看墙壁时的顺时针生成顺序
        WallTris.Add(Base); WallTris.Add(Base + 1); WallTris.Add(Base + 2);
        WallTris.Add(Base); WallTris.Add(Base + 2); WallTris.Add(Base + 3);

        for (int32 j = 0; j < 4; ++j) WallNormals.Add(OutNormal);

        const float HeightV = FoundationHeight * 0.01f;
        const float NextU = UVOffset + EdgeLen * 0.01f;
        WallUVs.Add(FVector2D(UVOffset,  0.0f));
        WallUVs.Add(FVector2D(NextU,     0.0f));
        WallUVs.Add(FVector2D(NextU,     HeightV));
        WallUVs.Add(FVector2D(UVOffset,  HeightV));
        UVOffset = NextU;
    }

    // ================= 4. 合并所有网格体数据 =================
    TArray<FVector> FoundationVerts;
    TArray<int32> FoundationTris;
    TArray<FVector> FoundationNormals;
    TArray<FVector2D> FoundationUVs;

    // 添加顶面
    FoundationVerts.Append(TopVerts);
    FoundationTris.Append(TopTris);
    FoundationNormals.Append(TopNormals);
    FoundationUVs.Append(TopUVs);

    // 添加底面
    const int32 BotBase = FoundationVerts.Num();
    FoundationVerts.Append(BotVerts);
    FoundationNormals.Append(BotNormals);
    FoundationUVs.Append(BotUVs);
    for (int32& Idx : BotTris) { Idx += BotBase; }
    FoundationTris.Append(BotTris);

    // 添加侧墙
    const int32 WallBase = FoundationVerts.Num();
    FoundationVerts.Append(WallVerts);
    FoundationNormals.Append(WallNormals);
    FoundationUVs.Append(WallUVs);
    for (int32& Idx : WallTris) { Idx += WallBase; }
    FoundationTris.Append(WallTris);

    // 创建 Mesh
    FoundationMesh = NewObject<UProceduralMeshComponent>(Owner);
    FoundationMesh->SetupAttachment(Owner->GetRootComponent());
    FoundationMesh->SetRelativeLocation(FVector::ZeroVector);
    {
        FoundationMesh->SetRelativeScale3D(FVector(
            InOwnerScale.X > KINDA_SMALL_NUMBER ? 1.0f / InOwnerScale.X : 1.0f,
            InOwnerScale.Y > KINDA_SMALL_NUMBER ? 1.0f / InOwnerScale.Y : 1.0f,
            1.0f));
    }
    FoundationMesh->CreateMeshSection_LinearColor(0, FoundationVerts, FoundationTris, FoundationNormals, FoundationUVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	
	if (FoundationMaterial)
	{
		FoundationMesh->SetMaterial(0, FoundationMaterial);
	}
	if (!FoundationCollisionProfileName.IsNone())
	{
		FoundationMesh->SetCollisionProfileName(FoundationCollisionProfileName);
	}
	FoundationMesh->RegisterComponent();

    BuildSidewalk(Owner, HW, HH, InOwnerScale);
}

void UFoundationComponent::BuildSidewalk(AActor* Owner, float HW, float HH, const FVector& InOwnerScale)
{
    if (SidewalkWidth <= 0.0f)
    {
        return;
    }

    const float OX = HW;
    const float OY = HH;
    const float IX = FMath::Max(0.0f, HW - SidewalkWidth);
    const float IY = FMath::Max(0.0f, HH - SidewalkWidth);

    const float TopZ = FoundationHeight + SidewalkHeight;
    const float BotZ = FoundationHeight;

    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;

    auto AddWall = [&](float X0, float Y0, float X1, float Y1, const FVector& Nrm)
    {
        const int32 Base = V.Num();
        V.Add(FVector(X0, Y0, TopZ));
        V.Add(FVector(X1, Y1, TopZ));
        V.Add(FVector(X1, Y1, BotZ));
        V.Add(FVector(X0, Y0, BotZ));
        // 修正：顺时针环绕
        T.Add(Base); T.Add(Base + 1); T.Add(Base + 2);
        T.Add(Base); T.Add(Base + 2); T.Add(Base + 3);
        for (int32 i = 0; i < 4; ++i) N.Add(Nrm);
    };

    const FVector Up(0, 0, 1);
    const FVector Down(0, -1, 0);
    const FVector Fwd(0, 1, 0);
    const FVector Right(1, 0, 0);
    const FVector Left(-1, 0, 0);

    // 外墙
    AddWall(-OX, -OY,  OX, -OY, Down);
    AddWall( OX, -OY,  OX,  OY, Right);
    AddWall( OX,  OY, -OX,  OY, Fwd);
    AddWall(-OX,  OY, -OX, -OY, Left);

    // 内墙
    AddWall( IX, -IY, -IX, -IY, Fwd);
    AddWall( IX,  IY,  IX, -IY, Left);
    AddWall(-IX,  IY,  IX,  IY, Down);
    AddWall(-IX, -IY, -IX,  IY, Right);

    // 顶面
    auto AddTop = [&](float X0, float Y0, float X1, float Y1, float X2, float Y2, float X3, float Y3)
    {
        const int32 Base = V.Num();
        V.Add(FVector(X0, Y0, TopZ));
        V.Add(FVector(X1, Y1, TopZ));
        V.Add(FVector(X2, Y2, TopZ));
        V.Add(FVector(X3, Y3, TopZ));
        // 修正：将原逆时针翻转为顺时针
        T.Add(Base); T.Add(Base + 2); T.Add(Base + 1);
        T.Add(Base); T.Add(Base + 3); T.Add(Base + 2);
        for (int32 i = 0; i < 4; ++i) N.Add(Up);
    };

    AddTop(-OX, -OY,  OX, -OY,  IX, -IY, -IX, -IY);
    AddTop( OX, -OY,  OX,  OY,  IX,  IY,  IX, -IY);
    AddTop( OX,  OY, -OX,  OY, -IX,  IY,  IX,  IY);
    AddTop(-OX,  OY, -OX, -OY, -IX, -IY, -IX,  IY);

    // 补充：为了完全封闭无死角，我们也加上人行道的底面
    auto AddBottom = [&](float X0, float Y0, float X1, float Y1, float X2, float Y2, float X3, float Y3)
    {
        const int32 Base = V.Num();
        V.Add(FVector(X0, Y0, BotZ));
        V.Add(FVector(X1, Y1, BotZ));
        V.Add(FVector(X2, Y2, BotZ));
        V.Add(FVector(X3, Y3, BotZ));
        // 底面朝下，环绕顺序需要和顶面相反
        T.Add(Base); T.Add(Base + 1); T.Add(Base + 2);
        T.Add(Base); T.Add(Base + 2); T.Add(Base + 3);
        for (int32 i = 0; i < 4; ++i) N.Add(Down);
    };

    AddBottom(-OX, -OY,  OX, -OY,  IX, -IY, -IX, -IY);
    AddBottom( OX, -OY,  OX,  OY,  IX,  IY,  IX, -IY);
    AddBottom( OX,  OY, -OX,  OY, -IX,  IY,  IX,  IY);
    AddBottom(-OX,  OY, -OX, -OY, -IX, -IY, -IX,  IY);

    TArray<FVector2D> UVs;
    UVs.SetNum(V.Num());
    for (int32 i = 0; i < V.Num(); ++i)
    {
        UVs[i] = FVector2D(V[i].X * 0.01f, V[i].Y * 0.01f);
    }

    SidewalkMesh = NewObject<UProceduralMeshComponent>(Owner);
    SidewalkMesh->SetupAttachment(Owner->GetRootComponent());
    SidewalkMesh->SetRelativeLocation(FVector::ZeroVector);
    {
        SidewalkMesh->SetRelativeScale3D(FVector(
            InOwnerScale.X > KINDA_SMALL_NUMBER ? 1.0f / InOwnerScale.X : 1.0f,
            InOwnerScale.Y > KINDA_SMALL_NUMBER ? 1.0f / InOwnerScale.Y : 1.0f,
            1.0f));
    }
    SidewalkMesh->CreateMeshSection_LinearColor(0, V, T, N, UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	
	if (SidewalkMaterial)
	{
		SidewalkMesh->SetMaterial(0, SidewalkMaterial);
	}
	if (!SidewalkCollisionProfileName.IsNone())
	{
		SidewalkMesh->SetCollisionProfileName(SidewalkCollisionProfileName);
	}
	SidewalkMesh->RegisterComponent();
}

void UFoundationComponent::ClearFoundation()
{
    if (FoundationMesh && IsValid(FoundationMesh))
    {
        FoundationMesh->DestroyComponent();
    }
    FoundationMesh = nullptr;

    if (SidewalkMesh && IsValid(SidewalkMesh))
    {
        SidewalkMesh->DestroyComponent();
    }
    SidewalkMesh = nullptr;
}

float UFoundationComponent::GetEdgePad(float BasePad, bool bConnected) const
{
    return bConnected ? 0.0f : BasePad;
}

void UFoundationComponent::GenerateOutline(const FEdgeConnection& Conn, float HW, float HH,
    TArray<FVector2D>& OutOutline) const
{
    const float PadN = GetEdgePad(Padding, Conn.bTop);
    const float PadE = GetEdgePad(Padding, Conn.bRight);
    const float PadS = GetEdgePad(Padding, Conn.bBottom);
    const float PadW = GetEdgePad(Padding, Conn.bLeft);

    auto ComputeR = [this](float PadA, float PadB) -> float
    {
        if (PadA <= 0.0f && PadB <= 0.0f) return 0.0f;
        const float MaxPad = FMath::Max(PadA, PadB);
        const float MinPad = FMath::Min(PadA, PadB);
        if (MinPad <= 0.0f)
        {
            if (FMath::IsNearlyZero(CornerRadius)) return 0.0f;
            return FMath::Min(CornerRadius, MaxPad);
        }
        return FMath::Min3(CornerRadius, PadA, PadB);
    };

    const float R_NE = ComputeR(PadE, PadN);
    const float R_NW = ComputeR(PadW, PadN);
    const float R_SW = ComputeR(PadW, PadS);
    const float R_SE = ComputeR(PadE, PadS);

    const int32 ArcSegs = 8;

    OutOutline.Empty();

    const float Top = HH - PadN;
    const float Bottom = -(HH - PadS);
    const float Left = -(HW - PadW);
    const float Right = HW - PadE;

    if (R_NE > 0.0f)
    {
        const FVector2D C(Right - R_NE, Top - R_NE);
        AddCornerArc(OutOutline, C, R_NE, 0.0f, HALF_PI, ArcSegs);
    }
    OutOutline.Add(FVector2D(Right - R_NE, Top));
    OutOutline.Add(FVector2D(Left + R_NW, Top));

    if (R_NW > 0.0f)
    {
        const FVector2D C(Left + R_NW, Top - R_NW);
        AddCornerArc(OutOutline, C, R_NW, HALF_PI, PI, ArcSegs);
    }
    OutOutline.Add(FVector2D(Left, Top - R_NW));
    OutOutline.Add(FVector2D(Left, Bottom + R_SW));

    if (R_SW > 0.0f)
    {
        const FVector2D C(Left + R_SW, Bottom + R_SW);
        AddCornerArc(OutOutline, C, R_SW, PI, 3.0f * HALF_PI, ArcSegs);
    }
    OutOutline.Add(FVector2D(Left + R_SW, Bottom));
    OutOutline.Add(FVector2D(Right - R_SE, Bottom));

    if (R_SE > 0.0f)
    {
        const FVector2D C(Right - R_SE, Bottom + R_SE);
        AddCornerArc(OutOutline, C, R_SE, 3.0f * HALF_PI, 2.0f * PI, ArcSegs);
    }
    OutOutline.Add(FVector2D(Right, Bottom + R_SE));
    OutOutline.Add(FVector2D(Right, Top - R_NE));
}

void UFoundationComponent::AddCornerArc(TArray<FVector2D>& OutVerts, FVector2D ArcCenter, float R,
    float StartAngle, float EndAngle, int32 Segments) const
{
    if (R <= 0.0f || Segments <= 0)
    {
        return;
    }

    const float Step = (EndAngle - StartAngle) / static_cast<float>(Segments);
    for (int32 i = 0; i <= Segments; ++i)
    {
        const float A = StartAngle + Step * i;
        OutVerts.Add(ArcCenter + FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R));
    }
}
