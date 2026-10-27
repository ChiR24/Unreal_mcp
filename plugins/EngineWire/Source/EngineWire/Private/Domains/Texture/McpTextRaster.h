// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/TextLayout.h"

namespace McpTextureHandlers
{
/** Text drawn white at its own size, centred in Size, offscreen by Slate (the editor UI's own rasterizer, so any font
 *  draws as it does in a widget): only the alpha of OutPixels is used, each pixel's coverage. False when the frame could
 *  not be read back. create_text_texture's text and contact_sheet's labels (McpTextureText.cpp). */
bool McpRasterizeText(const FString& Text, const FSlateFontInfo& Font, ETextJustify::Type Justify, FIntPoint Size,
                      TArray<FColor>& OutPixels);
} // namespace McpTextureHandlers
