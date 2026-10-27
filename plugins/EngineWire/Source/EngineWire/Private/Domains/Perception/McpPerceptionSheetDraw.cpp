// Copyright (c) 2024 EngineWire Contributors
//
// inspect contact_sheet's image: each tile shaded as clay from its depth and normal passes, so its shape reads the same
// in a level lit by one sun as in a studio, laid out three across with each label above its tile and its grid over it.

#include "Domains/Perception/McpPerceptionSheet.h"

#include "Domains/Texture/McpTextRaster.h"
#include "Styling/CoreStyle.h"

namespace McpPerception::Sheet
{
namespace
{
constexpr int32 SheetColumns = 3;
constexpr int32 StripHeight = 26; // the title's band and each label's
constexpr float Ambient = 0.3f;
constexpr float XRay = 0.45f; // how much of the subject shows through what hides it
constexpr float GridAlpha = 0.35f;
const FLinearColor SubjectAlbedo(0.95f, 0.5f, 0.15f);
const FLinearColor OtherAlbedo(0.62f, 0.62f, 0.64f);
const FColor SheetBackground(26, 26, 28);
const FColor TileBackground(44, 48, 56);

uint8 Encode(float Linear)
{
    const float Value = FMath::Clamp(Linear, 0.f, 1.f);
    const float Encoded = Value <= 0.0031308f ? Value * 12.92f : 1.055f * FMath::Pow(Value, 1.f / 2.4f) - 0.055f;
    return uint8(FMath::RoundToInt(Encoded * 255.f));
}

FColor Over(const FColor& Under, const FColor& Colour, float Alpha)
{
    return FColor(uint8(FMath::Lerp(float(Under.R), float(Colour.R), Alpha)), uint8(FMath::Lerp(float(Under.G), float(Colour.G), Alpha)),
                  uint8(FMath::Lerp(float(Under.B), float(Colour.B), Alpha)), 255);
}

// White text centred in a band of the sheet, by its coverage from the editor's own text renderer.
void DrawLabel(TArray<FColor>& Sheet, int32 SheetWidth, const FIntRect& Band, const FString& Text)
{
    TArray<FColor> Coverage;
    if (!McpTextureHandlers::McpRasterizeText(Text, FCoreStyle::GetDefaultFontStyle("Regular", 10), ETextJustify::Center,
                                              Band.Size(), Coverage))
    {
        return;
    }
    for (int32 Y = 0; Y < Band.Height(); ++Y)
    {
        for (int32 X = 0; X < Band.Width(); ++X)
        {
            FColor& Pixel = Sheet[(Band.Min.Y + Y) * SheetWidth + Band.Min.X + X];
            Pixel = Over(Pixel, FColor::White, Coverage[Y * Band.Width() + X].A / 255.f);
        }
    }
}
} // namespace

TArray<FColor> Shade(const TArray<FLinearColor>& Normals, const TArray<FLinearColor>& Depth, const TArray<FLinearColor>& Alone,
                     const FVector& Forward, const FVector& Right, const FVector& Up)
{
    const FVector3f Light = FVector3f((Up * 0.7 - Forward * 0.6 - Right * 0.5).GetSafeNormal());
    // Where nothing is drawn the depth is the far plane's: huge in perspective but finite in an orthographic view, and
    // the normal there is stale, so emptiness is read off the depth. The tile's corners never hold the subject, so its
    // own pass's deepest value is the far plane.
    float Far = 0.f;
    for (const FLinearColor& Pixel : Alone)
    {
        Far = FMath::Max(Far, Pixel.R);
    }
    const float Empty = Far - FMath::Max(1.f, Far * 1e-5f);
    TArray<FColor> Out;
    Out.SetNumUninitialized(Normals.Num());
    for (int32 Index = 0; Index < Normals.Num(); ++Index)
    {
        const FVector3f Normal(Normals[Index].R, Normals[Index].G, Normals[Index].B);
        const float Near = Depth[Index].R, Own = Alone[Index].R;
        if (Near >= Empty)
        {
            Out[Index] = TileBackground;
            continue;
        }
        const bool bOwn = Own < Empty && FMath::Abs(Own - Near) <= FMath::Max(1.f, 0.002f * Near);
        const float Lit = Ambient + (1.f - Ambient) * FMath::Max(0.f, FVector3f::DotProduct(Normal, Light));
        FLinearColor Colour = (bOwn ? SubjectAlbedo : OtherAlbedo) * Lit;
        if (!bOwn && Own < Empty && Own > Near)
        {
            Colour = FMath::Lerp(Colour, SubjectAlbedo * 0.8f, XRay);
        }
        Out[Index] = FColor(Encode(Colour.R), Encode(Colour.G), Encode(Colour.B), 255);
    }
    return Out;
}

TArray<FColor> Compose(const FString& Title, const TArray<FTile>& Tiles, int32 TileSize, int32& OutWidth, int32& OutHeight)
{
    OutWidth = FMath::Min(Tiles.Num(), SheetColumns) * TileSize;
    OutHeight = StripHeight + FMath::DivideAndRoundUp(Tiles.Num(), SheetColumns) * (StripHeight + TileSize);
    TArray<FColor> Sheet;
    Sheet.Init(SheetBackground, OutWidth * OutHeight);
    for (int32 Index = 0; Index < Tiles.Num(); ++Index)
    {
        const FTile& Tile = Tiles[Index];
        const int32 Left = (Index % SheetColumns) * TileSize;
        const int32 LabelTop = StripHeight + (Index / SheetColumns) * (StripHeight + TileSize);
        const int32 Top = LabelTop + StripHeight;
        for (int32 Y = 0; Y < TileSize; ++Y)
        {
            for (int32 X = 0; X < TileSize; ++X)
            {
                Sheet[(Top + Y) * OutWidth + Left + X] = Tile.Pixels[Y * TileSize + X];
            }
        }
        for (const float Column : Tile.Columns)
        {
            const int32 X = FMath::RoundToInt(Column);
            for (int32 Y = 0; Y < TileSize && X >= 0 && X < TileSize; ++Y)
            {
                FColor& Pixel = Sheet[(Top + Y) * OutWidth + Left + X];
                Pixel = Over(Pixel, FColor::White, GridAlpha);
            }
        }
        for (const float Row : Tile.Rows)
        {
            const int32 Y = FMath::RoundToInt(Row);
            for (int32 X = 0; X < TileSize && Y >= 0 && Y < TileSize; ++X)
            {
                FColor& Pixel = Sheet[(Top + Y) * OutWidth + Left + X];
                Pixel = Over(Pixel, FColor::White, GridAlpha);
            }
        }
        DrawLabel(Sheet, OutWidth, FIntRect(Left, LabelTop, Left + TileSize, Top), Tile.Label);
    }
    DrawLabel(Sheet, OutWidth, FIntRect(0, 0, OutWidth, StripHeight), Title);
    return Sheet;
}
} // namespace McpPerception::Sheet
