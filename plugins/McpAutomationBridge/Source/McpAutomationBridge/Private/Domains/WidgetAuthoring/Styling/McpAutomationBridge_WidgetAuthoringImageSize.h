#pragma once

#include "CoreMinimal.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Components/Image.h"
#include "Styling/SlateBrush.h"

// A UImage's size lives in its brush (ImageSize). SetDesiredSizeOverride only reaches the
// live Slate widget, so on a Widget Blueprint at design time it changed nothing that was
// saved: brushSize and every sized spec image fell back to 32 x 32.
inline void McpSetImageSize(UImage *Image, const FVector2D &Size) {
  FSlateBrush Brush = MCP_UIMAGE_GET_BRUSH(Image);
  Brush.ImageSize = Size;
  Image->SetBrush(Brush);
}
