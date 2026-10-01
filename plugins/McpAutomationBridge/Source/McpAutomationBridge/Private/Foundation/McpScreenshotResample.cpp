#include "Foundation/McpScreenshotResample.h"

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "ShaderCompiler.h"

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

bool McpScreenshotReturnsImage(const TSharedPtr<FJsonObject> &Payload) {
  bool bReturnBase64 = true;
  if (Payload.IsValid()) {
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);
  }
  return bReturnBase64;
}

bool ResolveScreenshotResolutionForMcp(const TSharedPtr<FJsonObject> &Payload,
                                       FIntPoint SourceSize, FIntPoint &OutSize,
                                       FString &OutError) {
  OutSize = SourceSize;
  if (!Payload.IsValid()) {
    return true;
  }

  FString Resolution;
  Payload->TryGetStringField(TEXT("resolution"), Resolution);
  Resolution = Resolution.TrimStartAndEnd().ToLower();

  // The schema also declares numeric width/height. Previously only the "WxH"
  // string form was read, so a caller passing width/height got a silent
  // no-op at native viewport size thinking the resize had been honoured.
  if (Resolution.IsEmpty()) {
    double WidthNumber = 0.0;
    double HeightNumber = 0.0;
    const bool bHasWidth = Payload->TryGetNumberField(TEXT("width"), WidthNumber);
    const bool bHasHeight = Payload->TryGetNumberField(TEXT("height"), HeightNumber);
    if (bHasWidth || bHasHeight) {
      if (!bHasWidth || !bHasHeight) {
        OutError = TEXT("width and height must be supplied together (or use resolution \"WxH\").");
        return false;
      }
      Resolution = FString::Printf(TEXT("%dx%d"),
                                   FMath::Max(1, static_cast<int32>(WidthNumber)),
                                   FMath::Max(1, static_cast<int32>(HeightNumber)));
    }
  }

  if (Resolution.IsEmpty() && McpScreenshotReturnsImage(Payload)) {
    Resolution = McpInlineScreenshotBox;
  }
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

bool ResolveScreenshotDirectoryForMcp(const TSharedPtr<FJsonObject> &Payload,
                                      const FString &DefaultDir, FString &OutDir,
                                      FString &OutError) {
  FString Raw;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("path"), Raw);
  }
  Raw.TrimStartAndEndInline();
  if (Raw.IsEmpty()) {
    OutDir = DefaultDir;
    return true;
  }
  // The shared project-file resolver: containment, traversal and reserved
  // device names are checked in one place for every file the plugin writes.
  return McpResolveProjectFilePath(Raw, OutDir, OutError);
}

int32 McpShaderJobsRemaining() {
  return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
}

void McpAddShaderCompileState(const TSharedPtr<FJsonObject> &Resp,
                              const TSharedPtr<FJsonObject> &Payload) {
  if (!Resp.IsValid()) {
    return;
  }
  const int32 Jobs = McpShaderJobsRemaining();
  Resp->SetNumberField(TEXT("shadersCompiling"), Jobs);
  const TSharedPtr<FJsonObject> *Waited = nullptr;
  if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("shaderWait"), Waited) && Waited) {
    Resp->SetObjectField(TEXT("shaderWait"), *Waited);
  }
  if (Jobs > 0) {
    // Added to whatever warnings the capture already carries (the game view's scene-only note).
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>> *Existing = nullptr;
    if (Resp->TryGetArrayField(TEXT("warnings"), Existing) && Existing) {
      Warnings = *Existing;
    }
    Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("%d shader job(s) were still compiling, so surfaces they cover are drawn with the engine's default ")
        TEXT("material (black or grey) and this picture may not show the final look. Retry with waitForShaders ")
        TEXT("true, or once shadersCompiling reads 0."),
        Jobs)));
    Resp->SetArrayField(TEXT("warnings"), Warnings);
  }
}

bool McpDeferForShaderCompile(const TSharedPtr<FJsonObject> &Payload,
                              TFunction<void(const TSharedPtr<FJsonObject> &)> Resume) {
  bool bWait = false;
  if (!Payload.IsValid() || !Payload->TryGetBoolField(TEXT("waitForShaders"), bWait) || !bWait ||
      Payload->HasField(TEXT("shaderWait")) || McpShaderJobsRemaining() == 0) {
    return false;
  }
  const double Start = FPlatformTime::Seconds();
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([Payload, Resume, Start](float) -> bool {
        const int32 Left = McpShaderJobsRemaining();
        const double Waited = FPlatformTime::Seconds() - Start;
        if (Left > 0 && Waited < McpShaderWaitMaxSeconds) {
          return true;
        }
        TSharedPtr<FJsonObject> Wait = MakeShared<FJsonObject>();
        Wait->SetNumberField(TEXT("waitedSeconds"), FMath::RoundToDouble(Waited * 10.0) / 10.0);
        Wait->SetNumberField(TEXT("jobsLeft"), Left);
        Wait->SetBoolField(TEXT("timedOut"), Left > 0);
        Payload->SetObjectField(TEXT("shaderWait"), Wait);
        Resume(Payload);
        return false;
      }),
      0.25f);
  return true;
}
