// Copyright (c) 2024 EngineWire Contributors
//
// analyze_view's colour: the palette (k-means in OKLab, a space where distances follow what the eye sees), how its hues
// sit together (the hue templates of Cohen-Or et al. 2006: sectors of the hue wheel that harmonious images keep to;
// scored only, never used to recolour, since template recolouring is claimed by later patents) and the cast of the
// surfaces that should be grey.

#include "Domains/Perception/McpPerception.h"

#include "Foundation/Solvers/McpKMeans.h"

namespace McpPerception
{
namespace
{
constexpr int32 PaletteSize = 5;
constexpr float Chromatic = 0.04f; // OKLab chroma below which a colour reads as grey
constexpr float CastShift = 0.015f;

struct FSector
{
    float Center;
    float Width;
};
struct FTemplate
{
    const TCHAR* Name;
    TArray<FSector> Sectors;
};

float Linear(float Value)
{
    return Value <= 0.04045f ? Value / 12.92f : FMath::Pow((Value + 0.055f) / 1.055f, 2.4f);
}

float Gamma(float Value)
{
    const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
    return Clamped <= 0.0031308f ? Clamped * 12.92f : 1.055f * FMath::Pow(Clamped, 1.f / 2.4f) - 0.055f;
}

FString Hex(const FVector3f& Lab)
{
    const float L = Lab.X + 0.3963377774f * Lab.Y + 0.2158037573f * Lab.Z;
    const float M = Lab.X - 0.1055613458f * Lab.Y - 0.0638541728f * Lab.Z;
    const float S = Lab.X - 0.0894841775f * Lab.Y - 1.2914855480f * Lab.Z;
    const float L3 = L * L * L, M3 = M * M * M, S3 = S * S * S;
    const FColor Shown(FMath::RoundToInt(255.f * Gamma(4.0767416621f * L3 - 3.3077115913f * M3 + 0.2309699292f * S3)),
                       FMath::RoundToInt(255.f * Gamma(-1.2684380046f * L3 + 2.6097574011f * M3 - 0.3413193965f * S3)),
                       FMath::RoundToInt(255.f * Gamma(-0.0041960863f * L3 - 0.7034186147f * M3 + 1.7076147010f * S3)));
    return FString::Printf(TEXT("#%02X%02X%02X"), Shown.R, Shown.G, Shown.B);
}

// How far, in degrees, a hue sits outside a template turned by Rotation.
float Outside(float Hue, const FTemplate& Template, float Rotation)
{
    float Best = 180.f;
    for (const FSector& Sector : Template.Sectors)
    {
        const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(Hue, Sector.Center + Rotation));
        Best = FMath::Min(Best, FMath::Max(0.f, Off - 0.5f * Sector.Width));
    }
    return Best;
}

TSharedPtr<FJsonObject> Harmony(const TArray<double>& Hues, double Weight, int32 Cells, int32 Total)
{
    static const TArray<FTemplate> Templates = {
        {TEXT("i"), {{0.f, 18.f}}}, {TEXT("V"), {{0.f, 93.6f}}}, {TEXT("L"), {{0.f, 18.f}, {90.f, 79.2f}}},
        {TEXT("I"), {{0.f, 18.f}, {180.f, 18.f}}}, {TEXT("T"), {{0.f, 180.f}}}, {TEXT("Y"), {{0.f, 93.6f}, {180.f, 18.f}}},
        {TEXT("X"), {{0.f, 93.6f}, {180.f, 93.6f}}}};
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    const double Share = Total > 0 ? double(Cells) / Total : 0.0;
    Out->SetNumberField(TEXT("chromatic"), FMath::RoundToDouble(1000.0 * Share) / 10.0);
    if (Share < 0.1 || Weight <= 0.0)
    {
        Out->SetStringField(TEXT("verdict"), TEXT("near grey: too little colour for hue harmony to say anything"));
        return Out;
    }
    double BestScore = TNumericLimits<double>::Max();
    const FTemplate* BestTemplate = &Templates[0];
    int32 BestRotation = 0;
    for (const FTemplate& Template : Templates)
    {
        for (int32 Rotation = 0; Rotation < 360; Rotation += 2)
        {
            double Score = 0.0;
            for (int32 Bin = 0; Bin < 360 && Score < BestScore * Weight; ++Bin)
            {
                Score += Hues[Bin] > 0.0 ? Hues[Bin] * Outside(Bin + 0.5f, Template, float(Rotation)) : 0.0;
            }
            if (Score / Weight < BestScore)
            {
                BestScore = Score / Weight;
                BestTemplate = &Template;
                BestRotation = Rotation;
            }
        }
    }
    Out->SetStringField(TEXT("template"), BestTemplate->Name);
    Out->SetNumberField(TEXT("rotation"), BestRotation);
    Out->SetNumberField(TEXT("offBy"), FMath::RoundToDouble(10.0 * BestScore) / 10.0); // degrees, by chroma
    Out->SetStringField(TEXT("verdict"), BestScore <= 3.0 ? TEXT("harmonious") : BestScore <= 10.0 ? TEXT("mostly harmonious") : TEXT("hues clash"));
    return Out;
}
} // namespace

FVector3f OkLab(const FLinearColor& Srgb)
{
    const float R = Linear(Srgb.R), G = Linear(Srgb.G), B = Linear(Srgb.B);
    const float L = FMath::Pow(FMath::Max(0.f, 0.4122214708f * R + 0.5363325363f * G + 0.0514459929f * B), 1.f / 3.f);
    const float M = FMath::Pow(FMath::Max(0.f, 0.2119034982f * R + 0.6806995451f * G + 0.1073969566f * B), 1.f / 3.f);
    const float S = FMath::Pow(FMath::Max(0.f, 0.0883024619f * R + 0.2817188376f * G + 0.6299787005f * B), 1.f / 3.f);
    return FVector3f(0.2104542553f * L + 0.7936177850f * M - 0.0040720468f * S,
                     1.9779984951f * L - 2.4285922050f * M + 0.4505937099f * S,
                     0.0259040371f * L + 0.7827717662f * M - 0.8086757660f * S);
}

TSharedPtr<FJsonObject> Palette(const FFrame& Frame)
{
    const int32 Total = Frame.Lab.Num();
    TArray<TSharedPtr<FJsonValue>> Colors;
    for (const McpKMeans::FCluster& Cluster : McpKMeans::Cluster(Frame.Lab, nullptr, PaletteSize, 12, 5))
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("hex"), Hex(Cluster.Center));
        Row->SetNumberField(TEXT("share"), FMath::RoundToDouble(1000.0 * Cluster.Weight / FMath::Max(1, Total)) / 10.0);
        Colors.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<double> Hues;
    Hues.Init(0.0, 360);
    double Weight = 0.0, CastA = 0.0, CastB = 0.0;
    int32 Coloured = 0, Neutral = 0;
    for (const FVector3f& Lab : Frame.Lab)
    {
        const float Chroma = FMath::Sqrt(Lab.Y * Lab.Y + Lab.Z * Lab.Z);
        if (Chroma >= Chromatic)
        {
            const float Hue = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Lab.Z, Lab.Y)) + 360.f, 360.f);
            Hues[FMath::Clamp(int32(Hue), 0, 359)] += Chroma;
            Weight += Chroma;
            ++Coloured;
        }
        else if (Lab.X > 0.25f && Lab.X < 0.95f) // what should read grey, not too dark or blown out to judge
        {
            CastA += Lab.Y;
            CastB += Lab.Z;
            ++Neutral;
        }
    }
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetArrayField(TEXT("colors"), Colors);
    Out->SetObjectField(TEXT("harmony"), Harmony(Hues, Weight, Coloured, Total));
    TSharedPtr<FJsonObject> Cast = MakeShared<FJsonObject>();
    const double A = Neutral > 0 ? CastA / Neutral : 0.0, B = Neutral > 0 ? CastB / Neutral : 0.0;
    const bool bShift = Neutral >= Total / 50 && FMath::Sqrt(A * A + B * B) > CastShift;
    Cast->SetStringField(TEXT("tint"), !bShift ? TEXT("neutral") : FMath::Abs(B) >= FMath::Abs(A) ? (B > 0.0 ? TEXT("warm") : TEXT("cool"))
                                                             : (A > 0.0 ? TEXT("magenta") : TEXT("green")));
    Cast->SetNumberField(TEXT("a"), FMath::RoundToDouble(1000.0 * A) / 1000.0);
    Cast->SetNumberField(TEXT("b"), FMath::RoundToDouble(1000.0 * B) / 1000.0);
    Out->SetObjectField(TEXT("cast"), Cast);
    return Out;
}
} // namespace McpPerception
