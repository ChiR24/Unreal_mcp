#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#if WITH_EDITOR
#include "Misc/FileHelper.h"

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

void SendScreenshotReceiptForMcp(UMcpAutomationBridgeSubsystem *Subsystem,
                                 TSharedPtr<FMcpBridgeWebSocket> Socket,
                                 const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 const uint8 *PngData, int64 PngBytes,
                                 const FString &FullPath, const TCHAR *What) {
  const bool bSaved = FFileHelper::SaveArrayToFile(
      TArrayView<const uint8>(PngData, static_cast<int32>(PngBytes)), *FullPath);
  // Base64 is opt-in: a native 2040x949 viewport PNG is ~2 MB and always blew
  // the base64 cap, so a default-on flag made the DEFAULT call fail. A plain
  // capture returns path + metadata; returnBase64=true (optionally with
  // resolution= to downscale) asks for inline image data.
  bool bReturnBase64 = false;
  Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetNumberField(TEXT("sizeBytes"), PngBytes);
  Resp->SetNumberField(TEXT("fileSizeBytes"), PngBytes);
  Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
  if (bSaved) {
    Resp->SetStringField(TEXT("path"), FullPath);
    Resp->SetStringField(TEXT("screenshotPath"), FPaths::ConvertRelativePathToFull(FullPath));
  }
  AddScreenshotMetadataForMcp(Resp, Payload);

  FString Error;
  FString ErrorCode;
  if (!bSaved && !bReturnBase64) {
    Error = FString::Printf(TEXT("%s captured but failed to save to %s, and returnBase64=false leaves no image output."),
                            What, *FullPath);
    ErrorCode = TEXT("SAVE_FAILED");
  } else if (bReturnBase64 && PngBytes > MaxScreenshotPngBytesForBase64ForMcp) {
    Error = MakeScreenshotTooLargeMessageForMcp(static_cast<int32>(PngBytes));
    ErrorCode = TEXT("IMAGE_TOO_LARGE");
  }
  if (!ErrorCode.IsEmpty()) {
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), Error);
    Resp->SetStringField(TEXT("message"), Error);
    Subsystem->SendAutomationResponse(Socket, RequestId, false, Error, Resp, ErrorCode);
    return;
  }
  if (bReturnBase64) {
    Resp->SetStringField(TEXT("imageBase64"),
                         FBase64::Encode(PngData, static_cast<uint32>(PngBytes)));
  }
  const FString Message = bReturnBase64
      ? FString::Printf(TEXT("%s captured and returned as image/png base64."), What)
      : FString::Printf(TEXT("%s captured."), What);
  Resp->SetStringField(TEXT("message"), Message);
  Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
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

  // ForceRedrawWindow repaints Slate, but a level viewport only re-renders its
  // scene when it is invalidated or realtime is on. A backgrounded editor is
  // throttled and neither happens, so this capture returned a 3D view that
  // could be minutes old while reporting success - a camera move or an actor
  // edit read as having done nothing. Re-render the level viewports first.
  // Same guards as the game-viewport capture: game thread only, never
  // re-entering rendering.
  if (GEditor && IsInGameThread() && !IsInRenderingThread()) {
    GEditor->RedrawLevelEditingViewports(false);
  }
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
