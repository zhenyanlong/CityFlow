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
    SanitizeOutline(Outline);

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
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bCreateFoundationCollision);
	
	if (FoundationMaterial)
	{
		FoundationMesh->SetMaterial(0, FoundationMaterial);
	}
	if (!FoundationCollisionProfileName.IsNone())
	{
		FoundationMesh->SetCollisionProfileName(FoundationCollisionProfileName);
	}
	FoundationMesh->RegisterComponent();

    BuildSidewalk(Owner, Outline, InOwnerScale);
}

void UFoundationComponent::BuildSidewalk(AActor* Owner, const TArray<FVector2D>& FoundationOutline,
    const FVector& InOwnerScale)
{
    if (SidewalkWidth <= KINDA_SMALL_NUMBER || FoundationOutline.Num() < 3)
    {
        return;
    }

    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);
    for (const FVector2D& Point : FoundationOutline)
    {
        Min.X = FMath::Min(Min.X, Point.X);
        Min.Y = FMath::Min(Min.Y, Point.Y);
        Max.X = FMath::Max(Max.X, Point.X);
        Max.Y = FMath::Max(Max.Y, Point.Y);
    }

    const float FootprintMaxInset = FMath::Max(0.0f, FMath::Min(Max.X - Min.X, Max.Y - Min.Y) * 0.45f);

    // A sampled round corner stops being a valid inset once the offset reaches its
    // local radius. Find the largest convex inset instead of allowing those samples
    // to cross the arc centre and create long fan-shaped triangles.
    float SafeMaxInset = FootprintMaxInset;
    TArray<FVector2D> ValidationOutline;
    if (!BuildInsetOutline(FoundationOutline, SafeMaxInset, ValidationOutline))
    {
        float ValidInset = 0.0f;
        float InvalidInset = SafeMaxInset;
        for (int32 Iteration = 0; Iteration < 16; ++Iteration)
        {
            const float CandidateInset = (ValidInset + InvalidInset) * 0.5f;
            if (BuildInsetOutline(FoundationOutline, CandidateInset, ValidationOutline))
            {
                ValidInset = CandidateInset;
            }
            else
            {
                InvalidInset = CandidateInset;
            }
        }
        SafeMaxInset = FMath::Max(0.0f, ValidInset - 0.1f);
    }

    const float OuterInset = FMath::Clamp(SidewalkInset, 0.0f, SafeMaxInset);
    const float InnerInset = FMath::Clamp(OuterInset + SidewalkWidth, OuterInset, SafeMaxInset);
    const float ActualWidth = InnerInset - OuterInset;
    if (ActualWidth <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float BevelSize = FMath::Clamp(SidewalkBevelSize, 0.0f, ActualWidth * 0.45f);
    const float BevelHeight = FMath::Clamp(SidewalkBevelHeight, 0.0f, SidewalkHeight);
    const float TopZ = FoundationHeight + SidewalkHeight;
    const float BevelStartZ = TopZ - BevelHeight;
    const float BotZ = FoundationHeight - FMath::Max(0.0f, SidewalkEmbedDepth);

    TArray<FVector2D> OuterBase;
    TArray<FVector2D> InnerBase;
    TArray<FVector2D> OuterTop;
    TArray<FVector2D> InnerTop;
    if (!BuildInsetOutline(FoundationOutline, OuterInset, OuterBase)
        || !BuildInsetOutline(FoundationOutline, InnerInset, InnerBase)
        || !BuildInsetOutline(FoundationOutline, OuterInset + BevelSize, OuterTop)
        || !BuildInsetOutline(FoundationOutline, InnerInset - BevelSize, InnerTop))
    {
        return;
    }

    TArray<FVector> V;
    TArray<int32> T;
    TArray<FVector> N;
    TArray<FVector2D> UVs;

    auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D,
        const FVector& Normal)
    {
        const int32 Base = V.Num();
        V.Append({A, B, C, D});
        // ProceduralMesh front faces follow the same clockwise convention used by
        // FoundationMesh. The supplied normals describe the visible side, so the
        // geometric winding must be reversed from the conventional RH cross product.
        T.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
        for (int32 Index = 0; Index < 4; ++Index)
        {
            N.Add(Normal);
            UVs.Add(FVector2D(V[Base + Index].X, V[Base + Index].Y) * 0.01f);
        }
    };

    const int32 NumPoints = FoundationOutline.Num();
    for (int32 Index = 0; Index < NumPoints; ++Index)
    {
        const int32 Next = (Index + 1) % NumPoints;
        const FVector2D Edge = (OuterBase[Next] - OuterBase[Index]).GetSafeNormal();
        const FVector OuterNormal(Edge.Y, -Edge.X, 0.0f);
        const FVector InnerNormal(-Edge.Y, Edge.X, 0.0f);

        AddQuad(FVector(OuterBase[Index], BotZ), FVector(OuterBase[Next], BotZ),
            FVector(OuterBase[Next], BevelStartZ), FVector(OuterBase[Index], BevelStartZ), OuterNormal);
        AddQuad(FVector(InnerBase[Next], BotZ), FVector(InnerBase[Index], BotZ),
            FVector(InnerBase[Index], BevelStartZ), FVector(InnerBase[Next], BevelStartZ), InnerNormal);

        if (BevelSize > KINDA_SMALL_NUMBER && BevelHeight > KINDA_SMALL_NUMBER)
        {
            const FVector OuterBevelNormal = (OuterNormal * BevelHeight + FVector::UpVector * BevelSize).GetSafeNormal();
            const FVector InnerBevelNormal = (InnerNormal * BevelHeight + FVector::UpVector * BevelSize).GetSafeNormal();
            AddQuad(FVector(OuterBase[Index], BevelStartZ), FVector(OuterBase[Next], BevelStartZ),
                FVector(OuterTop[Next], TopZ), FVector(OuterTop[Index], TopZ), OuterBevelNormal);
            AddQuad(FVector(InnerBase[Next], BevelStartZ), FVector(InnerBase[Index], BevelStartZ),
                FVector(InnerTop[Index], TopZ), FVector(InnerTop[Next], TopZ), InnerBevelNormal);
        }

        AddQuad(FVector(OuterTop[Index], TopZ), FVector(OuterTop[Next], TopZ),
            FVector(InnerTop[Next], TopZ), FVector(InnerTop[Index], TopZ), FVector::UpVector);
        AddQuad(FVector(InnerBase[Index], BotZ), FVector(InnerBase[Next], BotZ),
            FVector(OuterBase[Next], BotZ), FVector(OuterBase[Index], BotZ), FVector::DownVector);
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
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), bCreateSidewalkCollision);
	
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

void UFoundationComponent::SanitizeOutline(TArray<FVector2D>& Outline) const
{
    TArray<FVector2D> CleanOutline;
    CleanOutline.Reserve(Outline.Num());

    for (const FVector2D& Point : Outline)
    {
        if (CleanOutline.IsEmpty() || !CleanOutline.Last().Equals(Point, 0.01f))
        {
            CleanOutline.Add(Point);
        }
    }

    if (CleanOutline.Num() > 1 && CleanOutline[0].Equals(CleanOutline.Last(), 0.01f))
    {
        CleanOutline.Pop(EAllowShrinking::No);
    }

    Outline = MoveTemp(CleanOutline);
}

bool UFoundationComponent::BuildInsetOutline(const TArray<FVector2D>& Source, float Inset,
    TArray<FVector2D>& OutOutline) const
{
    OutOutline.Reset(Source.Num());
    if (Source.Num() < 3 || Inset < 0.0f)
    {
        return false;
    }
    if (Inset <= KINDA_SMALL_NUMBER)
    {
        OutOutline = Source;
        return true;
    }

    auto Cross2D = [](const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    };

    for (int32 Index = 0; Index < Source.Num(); ++Index)
    {
        const FVector2D& Previous = Source[(Index - 1 + Source.Num()) % Source.Num()];
        const FVector2D& Current = Source[Index];
        const FVector2D& Next = Source[(Index + 1) % Source.Num()];
        const FVector2D PreviousDirection = (Current - Previous).GetSafeNormal();
        const FVector2D NextDirection = (Next - Current).GetSafeNormal();
        if (PreviousDirection.IsNearlyZero() || NextDirection.IsNearlyZero())
        {
            continue;
        }

        const FVector2D PreviousNormal(-PreviousDirection.Y, PreviousDirection.X);
        const FVector2D NextNormal(-NextDirection.Y, NextDirection.X);
        const FVector2D PreviousOffset = Current + PreviousNormal * Inset;
        const FVector2D NextOffset = Current + NextNormal * Inset;
        const float Denominator = Cross2D(PreviousDirection, NextDirection);

        FVector2D InsetPoint;
        if (FMath::Abs(Denominator) > KINDA_SMALL_NUMBER)
        {
            const float DistanceAlongPrevious = Cross2D(NextOffset - PreviousOffset, NextDirection) / Denominator;
            InsetPoint = PreviousOffset + PreviousDirection * DistanceAlongPrevious;
        }
        else
        {
            InsetPoint = Current + PreviousNormal * Inset;
        }

        const float MaxMiterLength = FMath::Max(1.0f, Inset * 4.0f);
        if (!FMath::IsFinite(InsetPoint.X) || !FMath::IsFinite(InsetPoint.Y)
            || FVector2D::Distance(Current, InsetPoint) > MaxMiterLength)
        {
            const FVector2D Bisector = (PreviousNormal + NextNormal).GetSafeNormal();
            const float Projection = FMath::Max(0.25f, FMath::Abs(FVector2D::DotProduct(Bisector, PreviousNormal)));
            InsetPoint = Current + Bisector * (Inset / Projection);
        }

        OutOutline.Add(InsetPoint);
    }

    if (OutOutline.Num() != Source.Num())
    {
        OutOutline.Reset();
        return false;
    }

    float SignedDoubleArea = 0.0f;
    for (int32 Index = 0; Index < OutOutline.Num(); ++Index)
    {
        const FVector2D& Current = OutOutline[Index];
        const FVector2D& Next = OutOutline[(Index + 1) % OutOutline.Num()];
        const FVector2D& AfterNext = OutOutline[(Index + 2) % OutOutline.Num()];
        const FVector2D Edge = Next - Current;
        const FVector2D NextEdge = AfterNext - Next;
        if (Edge.SizeSquared() <= 0.01f || Cross2D(Edge, NextEdge) < -0.01f)
        {
            OutOutline.Reset();
            return false;
        }
        SignedDoubleArea += Cross2D(Current, Next);
    }
    return SignedDoubleArea > KINDA_SMALL_NUMBER;
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

    const int32 ArcSegs = FMath::Clamp(CornerSegments, 1, 32);

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
