#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#if WITH_EDITOR
namespace {
bool IsUsableSlateWindowForMcp(const TSharedPtr<SWindow> &Window) {
  return Window.IsValid() && Window->IsVisible() && !Window->IsWindowMinimized();
}
}  // namespace

FString MakeSafeScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload) {
  FString Filename;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("filename"), Filename);
  }

  if (Filename.IsEmpty()) {
    Filename = FString::Printf(
        TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  Filename = FPaths::GetCleanFilename(Filename);
  if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) ||
      Filename.Contains(TEXT("\\"))) {
    Filename = FString::Printf(
        TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  if (!Filename.EndsWith(TEXT(".png"))) {
    Filename += TEXT(".png");
  }
  return Filename;
}

void AddScreenshotMetadataForMcp(const TSharedPtr<FJsonObject> &Resp,
                                 const TSharedPtr<FJsonObject> &Payload) {
  if (!Resp.IsValid() || !Payload.IsValid()) {
    return;
  }

  bool bIncludeMetadata = false;
  if (!Payload->TryGetBoolField(TEXT("includeMetadata"), bIncludeMetadata) ||
      !bIncludeMetadata) {
    return;
  }

  const TSharedPtr<FJsonObject> *Metadata = nullptr;
  if (Payload->TryGetObjectField(TEXT("metadata"), Metadata) && Metadata &&
      Metadata->IsValid()) {
    Resp->SetObjectField(TEXT("metadata"), *Metadata);
  }
}

FString MakeScreenshotTooLargeMessageForMcp(int32 SizeBytes) {
  // Naming resolution matters: "use a smaller viewport" is not something a
  // caller on the far side of the bridge can act on, so the old wording left
  // returnBase64=false -- i.e. no image at all -- as the only apparent way out.
  return FString::Printf(
      TEXT("Screenshot PNG is too large to return as base64 (%d bytes, max %d bytes). Retry with a smaller resolution (e.g. resolution=\"1280x720\") or returnBase64=false."),
      SizeBytes, MaxScreenshotPngBytesForBase64ForMcp);
}

TSharedPtr<SWindow> GetFullEditorSlateWindowForMcp() {
  if (!FSlateApplication::IsInitialized() ||
      !FSlateApplication::Get().CanDisplayWindows()) {
    return nullptr;
  }

  TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
  if (IsUsableSlateWindowForMcp(RootWindow)) {
    return RootWindow;
  }

#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule *LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(
                TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport =
          LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        TSharedPtr<SWindow> ViewportWindow =
            FSlateApplication::Get().FindWidgetWindow(
                ActiveViewport->AsWidget());
        if (IsUsableSlateWindowForMcp(ViewportWindow)) {
          return ViewportWindow;
        }
      }
    }
  }
#endif

  TSharedPtr<SWindow> ActiveWindow =
      FSlateApplication::Get().GetActiveTopLevelWindow();
  return IsUsableSlateWindowForMcp(ActiveWindow) ? ActiveWindow : nullptr;
}

bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError) {
  TSharedRef<SWidget> WindowWidget = Window;
  TArray<FColor> Bitmap;

  FSlateApplication::Get().ForceRedrawWindow(Window);
  if (!FSlateApplication::Get().TakeScreenshot(WindowWidget, Bitmap, OutSize) ||
      Bitmap.Num() == 0 || OutSize.X <= 0 || OutSize.Y <= 0) {
    OutError = TEXT("Failed to capture Slate window pixels");
    return false;
  }

  for (FColor &Pixel : Bitmap) {
    Pixel.A = 255;
  }

  const FIntPoint CapturedSize(OutSize.X, OutSize.Y);
  FIntPoint TargetSize = CapturedSize;
  if (!ResolveScreenshotResolutionForMcp(Payload, CapturedSize, TargetSize,
                                         OutError)) {
    return false;
  }
  if (TargetSize != CapturedSize &&
      Bitmap.Num() >= CapturedSize.X * CapturedSize.Y) {
    TArray<FColor> Resampled;
    ResampleBitmapForMcp(Bitmap, CapturedSize, Resampled, TargetSize);
    Bitmap = MoveTemp(Resampled);
    OutSize.X = TargetSize.X;
    OutSize.Y = TargetSize.Y;
  }

  IImageWrapperModule &ImageWrapperModule =
      FModuleManager::LoadModuleChecked<IImageWrapperModule>(
          FName("ImageWrapper"));
  TSharedPtr<IImageWrapper> ImageWrapper =
      ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
  if (!ImageWrapper.IsValid()) {
    OutError = TEXT("Failed to create PNG image wrapper");
    return false;
  }

  TArray<uint8> RawData;
  RawData.SetNumUninitialized(Bitmap.Num() * 4);
  for (int32 PixelIndex = 0; PixelIndex < Bitmap.Num(); ++PixelIndex) {
    const FColor &Pixel = Bitmap[PixelIndex];
    RawData[PixelIndex * 4 + 0] = Pixel.R;
    RawData[PixelIndex * 4 + 1] = Pixel.G;
    RawData[PixelIndex * 4 + 2] = Pixel.B;
    RawData[PixelIndex * 4 + 3] = Pixel.A;
  }

  if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), OutSize.X,
                            OutSize.Y, ERGBFormat::RGBA, 8)) {
    OutError =
        TEXT("Failed to prepare Slate window screenshot pixels for PNG encoding");
    return false;
  }

  OutPngData = ImageWrapper->GetCompressed(100);
  if (OutPngData.Num() == 0) {
    OutError = TEXT("Failed to encode Slate window screenshot as PNG");
    return false;
  }
  return true;
}
#endif
