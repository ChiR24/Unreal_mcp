#pragma once

#include "CoreMinimal.h"

#include "Components/Image.h"
#include "Styling/SlateBrush.h"

// A UImage's size lives in its brush (ImageSize). SetDesiredSizeOverride only reaches the
// live Slate widget, so on a Widget Blueprint at design time it changed nothing that was
// saved: brushSize and every sized spec image fell back to 32 x 32.
inline void McpSetImageSize(UImage *Image, const FVector2D &Size) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  FSlateBrush Brush = Image->GetBrush();
#else
  FSlateBrush Brush = Image->Brush;
#endif
  Brush.ImageSize = Size;
  Image->SetBrush(Brush);
}
