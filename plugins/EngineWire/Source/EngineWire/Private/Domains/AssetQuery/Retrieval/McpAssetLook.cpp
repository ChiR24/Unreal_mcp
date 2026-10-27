// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetQuery/Retrieval/McpAssetLook.h"

#include "Domains/Perception/McpPerception.h"
#include "Foundation/Solvers/McpKMeans.h"

namespace McpAssetRetrieval
{
namespace
{
constexpr float LookBackdropGap = 0.04f;   // least OKLab distance from the backdrop at which a pixel is drawn
constexpr float LookBackdropStray = 0.1f;  // a frame straying further from its median has no backdrop: all of it drawn
constexpr int32 LookColours = 4;
constexpr int32 LookEdgeBins = 8;
constexpr int32 LookSide = 64; // a stored thumbnail is 256 across or more: averaged down, it keeps colours and outline
constexpr double LookPi = 3.14159265358979323846;

FVector3f LookPixel(const TArray<uint8>& Bgra, int32 Index)
{
    return McpPerception::OkLab(FLinearColor(Bgra[Index * 4 + 2] / 255.f, Bgra[Index * 4 + 1] / 255.f, Bgra[Index * 4] / 255.f));
}

// The backdrop as the tile's frame shows it: its median colour, and how far a pixel may stray from that and still be
// backdrop: the gap, widened to what the frame itself strays (its 60th percentile: the editor draws mesh thumbnails on
// a checker whose two tones lie 0.03-0.04 apart, and a part of the asset may touch the frame). Unset when the frame
// strays too far to be a backdrop (a texture fills its tile).
TOptional<TPair<FVector3f, float>> LookBackdrop(const TArray<FVector3f>& Lab, int32 Width, int32 Height)
{
    TArray<FVector3f> Frame;
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const int32 X = Index % Width, Y = Index / Width;
        if (X == 0 || Y == 0 || X == Width - 1 || Y == Height - 1)
        {
            Frame.Add(Lab[Index]);
        }
    }
    FVector3f Median;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        TArray<float> Values;
        for (const FVector3f& Colour : Frame)
        {
            Values.Add(Colour[Axis]);
        }
        Values.Sort();
        Median[Axis] = Values[Values.Num() / 2];
    }
    TArray<float> Strays;
    for (const FVector3f& Colour : Frame)
    {
        Strays.Add(FVector3f::Dist(Colour, Median));
    }
    Strays.Sort();
    const float Stray = Strays[Strays.Num() * 3 / 5];
    if (Stray > LookBackdropStray)
    {
        return TOptional<TPair<FVector3f, float>>();
    }
    return TPair<FVector3f, float>(Median, FMath::Max(LookBackdropGap, 1.25f * Stray));
}

// Hu's seven invariants of a binary image's normalized central moments (Hu 1962), each kept as sign x (12 + log10 |h|)
// and 0 under 1e-12: a symmetric shape's higher invariants are zero give or take rounding, and a plain log of that noise
// swung by whole decades and flipped sign between a square and the same square.
TArray<float> LookHu(const TArray<uint8>& Mask, int32 Width, int32 Height)
{
    double M00 = 0.0, M10 = 0.0, M01 = 0.0;
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const double On = Mask[Y * Width + X];
            M00 += On;
            M10 += On * X;
            M01 += On * Y;
        }
    }
    const double Cx = M10 / FMath::Max(M00, 1.0);
    const double Cy = M01 / FMath::Max(M00, 1.0);
    double Mu[4][4] = {};
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            if (Mask[Y * Width + X])
            {
                const double Dx = X - Cx, Dy = Y - Cy;
                const double Px[4] = {1.0, Dx, Dx * Dx, Dx * Dx * Dx};
                const double Py[4] = {1.0, Dy, Dy * Dy, Dy * Dy * Dy};
                for (int32 P = 0; P < 4; ++P)
                {
                    for (int32 Q = 0; P + Q < 4; ++Q)
                    {
                        Mu[P][Q] += Px[P] * Py[Q];
                    }
                }
            }
        }
    }
    const auto Eta = [&Mu, M00](int32 P, int32 Q) { return Mu[P][Q] / FMath::Pow(FMath::Max(M00, 1.0), 1.0 + (P + Q) / 2.0); };
    const double N20 = Eta(2, 0), N02 = Eta(0, 2), N11 = Eta(1, 1), N30 = Eta(3, 0), N03 = Eta(0, 3), N21 = Eta(2, 1), N12 = Eta(1, 2);
    const double A = N30 + N12, B = N21 + N03, C = N30 - 3.0 * N12, D = 3.0 * N21 - N03;
    const double H[7] = {
        N20 + N02,
        FMath::Square(N20 - N02) + 4.0 * N11 * N11,
        C * C + D * D,
        A * A + B * B,
        C * A * (A * A - 3.0 * B * B) + D * B * (3.0 * A * A - B * B),
        (N20 - N02) * (A * A - B * B) + 4.0 * N11 * A * B,
        D * A * (A * A - 3.0 * B * B) - C * B * (3.0 * A * A - B * B)};
    TArray<float> Out;
    for (const double Value : H)
    {
        Out.Add(FMath::Abs(Value) < 1e-12 ? 0.f : static_cast<float>(FMath::Sign(Value) * (12.0 + FMath::LogX(10.0, FMath::Abs(Value)))));
    }
    return Out;
}
} // namespace

bool LookOf(const TArray<uint8>& Source, int32 SourceWidth, int32 SourceHeight, FLook& Out)
{
    if (SourceWidth < 8 || SourceHeight < 8 || Source.Num() < SourceWidth * SourceHeight * 4)
    {
        return false;
    }
    const int32 Step = FMath::Max(1, FMath::Min(SourceWidth, SourceHeight) / LookSide);
    const int32 Width = SourceWidth / Step, Height = SourceHeight / Step;
    TArray<uint8> Bgra; // Step x Step blocks averaged
    Bgra.SetNumUninitialized(Width * Height * 4);
    for (int32 Index = 0; Index < Width * Height * 4; ++Index)
    {
        const int32 X = (Index / 4) % Width, Y = (Index / 4) / Width;
        int32 Sum = 0;
        for (int32 Dy = 0; Dy < Step; ++Dy)
        {
            for (int32 Dx = 0; Dx < Step; ++Dx)
            {
                Sum += Source[((Y * Step + Dy) * SourceWidth + X * Step + Dx) * 4 + Index % 4];
            }
        }
        Bgra[Index] = static_cast<uint8>(Sum / (Step * Step));
    }
    TArray<FVector3f> Lab;
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        Lab.Add(LookPixel(Bgra, Index));
    }
    const TOptional<TPair<FVector3f, float>> Backdrop = LookBackdrop(Lab, Width, Height);
    TArray<uint8> Mask;
    Mask.Init(0, Width * Height);
    TArray<FVector3f> Drawn;
    TArray<float> Light;
    Light.Init(0.f, Width * Height);
    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const FVector3f Colour = Lab[Index];
        // The backdrop flattened to its median lightness: the edges are the asset's own, not the checker's around it.
        Light[Index] = Backdrop.IsSet() ? Backdrop->Key.X : Colour.X;
        if (!Backdrop.IsSet() || FVector3f::Dist(Colour, Backdrop->Key) > Backdrop->Value)
        {
            Mask[Index] = 1;
            Light[Index] = Colour.X;
            Drawn.Add(Colour);
        }
    }
    if (Drawn.Num() < 16)
    {
        return false;
    }
    Out.Drawn = static_cast<float>(Drawn.Num()) / (Width * Height);
    Out.Palette.Reset();
    for (const McpKMeans::FCluster& Cluster : McpKMeans::Cluster(Drawn, nullptr, LookColours, 12, 1))
    {
        Out.Palette.Add(FVector4f(Cluster.Center.X, Cluster.Center.Y, Cluster.Center.Z, static_cast<float>(Cluster.Weight / Drawn.Num())));
    }
    // Edge orientation (not direction: 0 and 180 degrees share a bin), Sobel on lightness inside the drawn part.
    Out.Edges.Init(0.f, LookEdgeBins);
    double Total = 0.0;
    for (int32 Y = 1; Y < Height - 1; ++Y)
    {
        for (int32 X = 1; X < Width - 1; ++X)
        {
            if (!Mask[Y * Width + X])
            {
                continue;
            }
            const auto L = [&Light, Width](int32 Px, int32 Py) { return Light[Py * Width + Px]; };
            const double Gx = (L(X + 1, Y - 1) + 2 * L(X + 1, Y) + L(X + 1, Y + 1)) - (L(X - 1, Y - 1) + 2 * L(X - 1, Y) + L(X - 1, Y + 1));
            const double Gy = (L(X - 1, Y + 1) + 2 * L(X, Y + 1) + L(X + 1, Y + 1)) - (L(X - 1, Y - 1) + 2 * L(X, Y - 1) + L(X + 1, Y - 1));
            const double Magnitude = FMath::Sqrt(Gx * Gx + Gy * Gy);
            const double Angle = FMath::Fmod(FMath::Atan2(Gy, Gx) + 2.0 * LookPi, LookPi);
            Out.Edges[FMath::Min(static_cast<int32>(Angle / LookPi * LookEdgeBins), LookEdgeBins - 1)] += static_cast<float>(Magnitude);
            Total += Magnitude;
        }
    }
    for (float& Bin : Out.Edges)
    {
        Bin = Total > 0.0 ? static_cast<float>(Bin / Total) : 1.f / LookEdgeBins;
    }
    Out.Hu = LookHu(Mask, Width, Height);
    return true;
}

double Unlike(const FLook& A, const FLook& B)
{
    // Each colour of one palette against the closest of the other's, weighted by its share, both ways.
    const auto Toward = [](const FLook& From, const FLook& To)
    {
        double Sum = 0.0;
        for (const FVector4f& Colour : From.Palette)
        {
            double Closest = 1.0;
            for (const FVector4f& Other : To.Palette)
            {
                Closest = FMath::Min(Closest, static_cast<double>(FVector3f::Dist(FVector3f(Colour.X, Colour.Y, Colour.Z), FVector3f(Other.X, Other.Y, Other.Z))));
            }
            Sum += Colour.W * Closest;
        }
        return Sum;
    };
    const double Palette = (Toward(A, B) + Toward(B, A)) / 2.0;
    double Edges = 0.0;
    for (int32 Bin = 0; Bin < A.Edges.Num() && Bin < B.Edges.Num(); ++Bin)
    {
        Edges += FMath::Abs(A.Edges[Bin] - B.Edges[Bin]);
    }
    double Shape = 0.0;
    for (int32 Index = 0; Index < 4 && Index < A.Hu.Num() && Index < B.Hu.Num(); ++Index) // the first four are the stable ones
    {
        Shape += FMath::Abs(A.Hu[Index] - B.Hu[Index]) / 4.0;
    }
    return 4.0 * Palette + 0.5 * Edges + 0.25 * Shape;
}

double ShareNear(const FLook& Look, const FVector3f& Colour, double DeltaE)
{
    double Share = 0.0;
    for (const FVector4f& Entry : Look.Palette)
    {
        Share += FVector3f::Dist(FVector3f(Entry.X, Entry.Y, Entry.Z), Colour) <= DeltaE ? Entry.W : 0.0;
    }
    return Share;
}
} // namespace McpAssetRetrieval
