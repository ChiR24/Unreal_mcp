#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"

// The pixel format a render target format name stands for: an engine ETextureRenderTargetFormat (RTF_RGBA16f, with or
// without the RTF_ prefix, case ignored) or one of the pixel-format spellings callers used before (FloatRGBA,
// A2B10G10R10). PF_Unknown when it names none.
inline EPixelFormat McpParseRenderTargetPixelFormat(const FString& Name)
{
    static const TMap<FString, FString> Aliases = {
        {TEXT("FloatRGBA"), TEXT("RGBA16f")}, {TEXT("A2B10G10R10"), TEXT("RGB10A2")}};
    const FString* Alias = Aliases.Find(Name);
    const FString FormatName = Alias ? *Alias : Name;
    const int64 Value = StaticEnum<ETextureRenderTargetFormat>()->GetValueByNameString(
        FormatName.StartsWith(TEXT("RTF_"), ESearchCase::IgnoreCase) ? FormatName : TEXT("RTF_") + FormatName);
    return Value == INDEX_NONE
        ? PF_Unknown
        : GetPixelFormatFromRenderTargetFormat(static_cast<ETextureRenderTargetFormat>(Value));
}
