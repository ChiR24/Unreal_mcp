// Copyright (c) 2024 EngineWire Contributors
//
// analyze_view's light: exposure as a photographer reads it (a lightness histogram, how much is blown out or crushed to
// black, the spread from the darker 5 percent to the brighter 5 percent) and, on a subject, the key to fill ratio: how
// many stops brighter its side facing the sun is drawn than its side turned away, from the world normals of the cells.

#include "Domains/Perception/McpPerception.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"

namespace McpPerception
{
namespace
{
constexpr int32 Bins = 8;
constexpr float Facing = 0.2f; // a normal this far toward (or away from) the sun counts as lit (or shadowed)
constexpr int32 SideCells = 3;  // fewer cells than this on a side cannot say how bright it is

double Luminance(const FLinearColor& Srgb)
{
    const FLinearColor Lin = FLinearColor::FromSRGBColor(Srgb.ToFColor(false));
    return 0.2126 * Lin.R + 0.7152 * Lin.G + 0.0722 * Lin.B;
}

double Percent(double Part, double Whole)
{
    return Whole > 0.0 ? FMath::RoundToDouble(1000.0 * Part / Whole) / 10.0 : 0.0;
}

// Every material the actor draws with is unlit: no light shades it, and the normal pass holds no faces of it (an unlit
// cube read 78% "toward the sun" while the camera saw its shadowed face).
bool DrawsUnlit(const AActor& Actor)
{
    TInlineComponentArray<UPrimitiveComponent*> Primitives(&Actor);
    bool bAny = false;
    for (const UPrimitiveComponent* Primitive : Primitives)
    {
        for (int32 Slot = 0; Slot < Primitive->GetNumMaterials(); ++Slot)
        {
            const UMaterialInterface* Material = Primitive->GetMaterial(Slot);
            if (Material && !Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit))
            {
                return false;
            }
            bAny |= Material != nullptr;
        }
    }
    return bAny;
}
} // namespace

TSharedPtr<FJsonObject> Light(const FFrame& Frame, const AActor* Subject, const TArray<FVector3f>& Normals, const FVector3f& ToSun)
{
    const int32 Count = Frame.Lab.Num();
    TArray<int32> Histogram;
    Histogram.Init(0, Bins);
    TArray<float> Lightness;
    Lightness.Reserve(Count);
    int32 Clipped = 0, Crushed = 0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float L = Frame.Lab[Index].X;
        const FLinearColor& Shown = Frame.Color[Index];
        ++Histogram[FMath::Clamp(int32(L * Bins), 0, Bins - 1)];
        Lightness.Add(L);
        Clipped += FMath::Max3(Shown.R, Shown.G, Shown.B) >= 0.98f ? 1 : 0;
        Crushed += FMath::Max3(Shown.R, Shown.G, Shown.B) <= 0.03f ? 1 : 0;
    }
    Lightness.Sort();
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Shares;
    for (const int32 Cells : Histogram)
    {
        Shares.Add(MakeShared<FJsonValueNumber>(Percent(Cells, Count)));
    }
    const double Spread = Count > 0 ? Lightness[Count * 95 / 100] - Lightness[Count * 5 / 100] : 0.0;
    const double Middle = Count > 0 ? Lightness[Count / 2] : 0.0;
    Out->SetArrayField(TEXT("histogram"), Shares);
    Out->SetNumberField(TEXT("clipped"), Percent(Clipped, Count));
    Out->SetNumberField(TEXT("crushed"), Percent(Crushed, Count));
    Out->SetNumberField(TEXT("contrast"), FMath::RoundToDouble(100.0 * Spread) / 100.0);
    Out->SetNumberField(TEXT("medianLightness"), FMath::RoundToDouble(100.0 * Middle) / 100.0);
    Out->SetBoolField(TEXT("flat"), Spread < 0.25);
    Out->SetStringField(TEXT("exposure"), Middle < 0.3 ? TEXT("dark") : Middle > 0.8 ? TEXT("bright") : TEXT("balanced"));
    if (!Subject || Normals.Num() != Count)
    {
        return Out;
    }
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("subject"), Row);
    if (DrawsUnlit(*Subject))
    {
        Row->SetStringField(TEXT("keyFillNote"), TEXT("the subject's materials are unlit, so no light shades it"));
        return Out;
    }
    double LitSum = 0.0, ShadowSum = 0.0;
    int32 Lit = 0, Shadow = 0, Cells = 0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Frame.Actor[Index] != Subject)
        {
            continue;
        }
        ++Cells;
        const float Toward = FVector3f::DotProduct(Normals[Index], ToSun);
        const double Y = Luminance(Frame.Color[Index]);
        LitSum += Toward > Facing ? Y : 0.0;
        Lit += Toward > Facing ? 1 : 0;
        ShadowSum += Toward < -Facing ? Y : 0.0;
        Shadow += Toward < -Facing ? 1 : 0;
    }
    Row->SetNumberField(TEXT("lit"), Percent(Lit, Cells));
    Row->SetNumberField(TEXT("shadowed"), Percent(Shadow, Cells));
    if (Lit >= SideCells && Shadow >= SideCells && ShadowSum > 0.0)
    {
        Row->SetNumberField(TEXT("keyFill"), FMath::RoundToDouble(10.0 * FMath::Log2((LitSum / Lit) / (ShadowSum / Shadow))) / 10.0);
    }
    else
    {
        Row->SetStringField(TEXT("keyFillNote"), Cells == 0 ? TEXT("the subject draws no cell in the frame")
                                                            : TEXT("the camera sees too little of one side to compare"));
    }
    return Out;
}
} // namespace McpPerception
