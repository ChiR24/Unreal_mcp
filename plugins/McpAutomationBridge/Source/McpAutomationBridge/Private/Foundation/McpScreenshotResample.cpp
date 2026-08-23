#include "Foundation/McpScreenshotResample.h"

namespace {
bool IsAllDigitsForMcp(const FString &Value) {
  if (Value.IsEmpty()) {
    return false;
  }
  for (const TCHAR Character : Value) {
    if (!FChar::IsDigit(Character)) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool ResolveScreenshotResolutionForMcp(const TSharedPtr<FJsonObject> &Payload,
                                       FIntPoint SourceSize, FIntPoint &OutSize,
                                       FString &OutError) {
  OutSize = SourceSize;

  FString Resolution;
  if (!Payload.IsValid() ||
      !Payload->TryGetStringField(TEXT("resolution"), Resolution)) {
    return true;
  }
  Resolution = Resolution.TrimStartAndEnd().ToLower();
  if (Resolution.IsEmpty()) {
    return true;
  }

  FString WidthPart;
  FString HeightPart;
  if (!Resolution.Split(TEXT("x"), &WidthPart, &HeightPart) ||
      !IsAllDigitsForMcp(WidthPart) || !IsAllDigitsForMcp(HeightPart)) {
    OutError = FString::Printf(
        TEXT("Invalid resolution \"%s\". Use WxH, for example 1280x720."),
        *Resolution);
    return false;
  }

  const int32 RequestedWidth = FCString::Atoi(*WidthPart);
  const int32 RequestedHeight = FCString::Atoi(*HeightPart);
  if (RequestedWidth <= 0 || RequestedHeight <= 0 || SourceSize.X <= 0 ||
      SourceSize.Y <= 0) {
    OutError = FString::Printf(
        TEXT("Invalid resolution \"%s\". Width and height must be positive."),
        *Resolution);
    return false;
  }

  const double Scale =
      FMath::Min(static_cast<double>(RequestedWidth) / SourceSize.X,
                 static_cast<double>(RequestedHeight) / SourceSize.Y);
  if (Scale >= 1.0) {
    // Asking for a box at least as big as the frame we already have means
    // "don't shrink it"; resampling upwards would invent detail.
    return true;
  }

  OutSize.X = FMath::Max(1, FMath::FloorToInt32(SourceSize.X * Scale));
  OutSize.Y = FMath::Max(1, FMath::FloorToInt32(SourceSize.Y * Scale));
  return true;
}

void ResampleBitmapForMcp(const TArray<FColor> &SrcBitmap, FIntPoint SrcSize,
                          TArray<FColor> &OutBitmap, FIntPoint DstSize) {
  OutBitmap.SetNumUninitialized(DstSize.X * DstSize.Y);

  // Area average rather than a nearest-neighbour pick: at the 3x-plus factors a
  // 4K viewport needs to fit the base64 budget, dropping pixels aliases thin
  // geometry -- track kerbs, wires, text -- into noise.
  for (int32 DstY = 0; DstY < DstSize.Y; ++DstY) {
    const int32 SrcY0 = (DstY * SrcSize.Y) / DstSize.Y;
    const int32 SrcY1 =
        FMath::Min(SrcSize.Y, FMath::Max(SrcY0 + 1,
                                         ((DstY + 1) * SrcSize.Y) / DstSize.Y));
    for (int32 DstX = 0; DstX < DstSize.X; ++DstX) {
      const int32 SrcX0 = (DstX * SrcSize.X) / DstSize.X;
      const int32 SrcX1 =
          FMath::Min(SrcSize.X, FMath::Max(SrcX0 + 1, ((DstX + 1) * SrcSize.X) /
                                                          DstSize.X));

      uint32 SumR = 0;
      uint32 SumG = 0;
      uint32 SumB = 0;
      uint32 Count = 0;
      for (int32 SrcY = SrcY0; SrcY < SrcY1; ++SrcY) {
        for (int32 SrcX = SrcX0; SrcX < SrcX1; ++SrcX) {
          const FColor &Pixel = SrcBitmap[SrcY * SrcSize.X + SrcX];
          SumR += Pixel.R;
          SumG += Pixel.G;
          SumB += Pixel.B;
          ++Count;
        }
      }

      OutBitmap[DstY * DstSize.X + DstX] =
          Count > 0 ? FColor(static_cast<uint8>(SumR / Count),
                             static_cast<uint8>(SumG / Count),
                             static_cast<uint8>(SumB / Count), 255)
                    : FColor(0, 0, 0, 255);
    }
  }
}
