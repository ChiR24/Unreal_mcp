#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  Mode = Mode.TrimStartAndEnd().ToLower();
  if (Mode.IsEmpty()) {
    Mode = TEXT("editor_viewport");
  }

  if (Mode == TEXT("game_viewport")) {
    return HandleUiAction(RequestId, TEXT("system_control"), Payload, Socket);
  }

  if (Mode != TEXT("editor_viewport") && Mode != TEXT("full_editor_window")) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        TEXT("Invalid screenshot mode. Supported modes: editor_viewport, game_viewport, full_editor_window"),
        nullptr);
    return true;
  }

  const FString Filename = MakeSafeScreenshotFilenameForMcp(Payload);

  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;

  if (Mode == TEXT("full_editor_window")) {
    TSharedPtr<SWindow> EditorWindow = GetFullEditorSlateWindowForMcp();
    if (!EditorWindow.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId,
                                TEXT("EDITOR_WINDOW_NOT_AVAILABLE"),
                                TEXT("No visible editor window available for full editor screenshot"),
                                nullptr);
      return true;
    }

    TArray<uint8> PngData;
    FIntVector ImageSize(0, 0, 0);
    FString CaptureError;
    if (!CaptureSlateWindowPngForMcp(EditorWindow.ToSharedRef(), Payload,
                                     PngData, ImageSize, CaptureError)) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                                CaptureError, nullptr);
      return true;
    }

    const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

    bool bReturnBase64 = true;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    Resp->SetNumberField(TEXT("width"), ImageSize.X);
    Resp->SetNumberField(TEXT("height"), ImageSize.Y);
    Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());
    Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    if (bSaved) {
      Resp->SetStringField(TEXT("path"), FullPath);
      Resp->SetStringField(TEXT("screenshotPath"), FullPath);
    }
    AddScreenshotMetadataForMcp(Resp, Payload);
    if (!bSaved && !bReturnBase64) {
      const FString SaveError = TEXT("Full editor window screenshot captured but failed to save, and returnBase64=false leaves no image output.");
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"), SaveError);
      Resp->SetStringField(TEXT("message"), SaveError);
      SendAutomationResponse(Socket, RequestId, false, SaveError, Resp,
                             TEXT("SAVE_FAILED"));
      return true;
    }
    if (bReturnBase64 && PngData.Num() > MaxScreenshotPngBytesForBase64ForMcp) {
      const FString SizeError = MakeScreenshotTooLargeMessageForMcp(PngData.Num());
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"), SizeError);
      Resp->SetStringField(TEXT("message"), SizeError);
      SendAutomationResponse(Socket, RequestId, false, SizeError, Resp,
                             TEXT("IMAGE_TOO_LARGE"));
      return true;
    }
    if (bReturnBase64) {
      Resp->SetStringField(TEXT("imageBase64"), FBase64::Encode(PngData));
    }
    Resp->SetStringField(TEXT("message"),
        bReturnBase64
            ? TEXT("Full editor window screenshot captured and returned as image/png base64.")
            : TEXT("Full editor window screenshot captured."));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Full editor window screenshot captured"), Resp,
                           FString());
    return true;
  }

  FViewport* Viewport = nullptr;
  FEditorViewportClient* CaptureClient = nullptr;
  if (GEditor->PlayWorld != nullptr && GEditor->GetPIEViewport() != nullptr) {
    Viewport = GEditor->GetPIEViewport();
  }
  if (!Viewport) {
    // Resolve through the same helper the camera handlers use, so the viewport
    // that gets moved is provably the viewport that gets photographed.
    CaptureClient = GetActiveEditorViewportClientForMcp();
    if (CaptureClient) {
      Viewport = CaptureClient->Viewport;
      CaptureClient->Invalidate();
    }
  }
  if (!Viewport) {
    Viewport = GEditor->GetActiveViewport();
  }
  if (!Viewport) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
                              TEXT("No active viewport available"), nullptr);
    return true;
  }

  const FIntPoint ViewportSize = Viewport->GetSizeXY();
  if (ViewportSize.X <= 0 || ViewportSize.Y <= 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_READY"),
                              TEXT("Viewport has zero size"), nullptr);
    return true;
  }

  Viewport->Draw();
  FlushRenderingCommands();

  TArray<FColor> Bitmap;
  const FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
  if (!Viewport->ReadPixels(Bitmap, ReadFlags) || Bitmap.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                              TEXT("Failed to read pixels from viewport"), nullptr);
    return true;
  }

  for (FColor& Pixel : Bitmap) {
    Pixel.A = 255;
  }

  // "resolution" was declared on this capability but never read, so a 4K
  // viewport could only ever answer IMAGE_TOO_LARGE no matter what the caller
  // asked for. Resample here and the parameter means what the schema says.
  FIntPoint OutputSize = ViewportSize;
  FString ResolutionError;
  if (!ResolveScreenshotResolutionForMcp(Payload, ViewportSize, OutputSize,
                                         ResolutionError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              ResolutionError, nullptr);
    return true;
  }

  TArray<FColor> ResampledBitmap;
  if (OutputSize != ViewportSize &&
      Bitmap.Num() >= ViewportSize.X * ViewportSize.Y) {
    ResampleBitmapForMcp(Bitmap, ViewportSize, ResampledBitmap, OutputSize);
    Bitmap = MoveTemp(ResampledBitmap);
  } else {
    OutputSize = ViewportSize;
  }

  TArray64<uint8> PngData;
  FImageUtils::PNGCompressImageArray(
      OutputSize.X,
      OutputSize.Y,
      TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()),
      PngData);
  if (PngData.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                              TEXT("Failed to encode viewport screenshot as PNG"), nullptr);
    return true;
  }

  const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

  // Defaulted to false while full_editor_window defaulted to true, so a plain
  // screenshot call handed back only a path and the caller had to go read the
  // PNG itself. The point of the capability is to return the picture; the
  // oversize guard below still protects the receipt.
  bool bReturnBase64 = true;
  Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("mode"), Mode);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  // width/height describe the PNG actually returned. When a resample happened
  // the untouched viewport size rides alongside, so a caller comparing the two
  // can tell a downscaled frame from a native-resolution one.
  Resp->SetNumberField(TEXT("width"), OutputSize.X);
  Resp->SetNumberField(TEXT("height"), OutputSize.Y);
  if (OutputSize != ViewportSize) {
    Resp->SetNumberField(TEXT("viewportWidth"), ViewportSize.X);
    Resp->SetNumberField(TEXT("viewportHeight"), ViewportSize.Y);
  }
  Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());
  Resp->SetNumberField(TEXT("fileSizeBytes"), PngData.Num());
  Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
  if (bSaved) {
    Resp->SetStringField(TEXT("path"), FullPath);
    Resp->SetStringField(TEXT("screenshotPath"), FullPath);
  }
  // Ship the camera with the picture. Without it a caller cannot tell a correct
  // frame from a frame taken somewhere else entirely, which is exactly how a
  // camera handler that silently ignored its arguments stayed hidden.
  if (CaptureClient) {
    Resp->SetObjectField(TEXT("cameraLocation"),
                         MakeVectorObjectForMcp(CaptureClient->GetViewLocation()));
    Resp->SetObjectField(TEXT("cameraRotation"),
                         MakeRotatorObjectForMcp(CaptureClient->GetViewRotation()));
  }
  AddScreenshotMetadataForMcp(Resp, Payload);

  if (!bSaved && !bReturnBase64) {
    const FString SaveError = FString::Printf(TEXT("Failed to save screenshot to %s"), *FullPath);
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), SaveError);
    Resp->SetStringField(TEXT("message"), SaveError);
    SendAutomationResponse(Socket, RequestId, false, SaveError, Resp,
                           TEXT("SAVE_FAILED"));
    return true;
  }
  if (bReturnBase64 && PngData.Num() > MaxScreenshotPngBytesForBase64ForMcp) {
    const FString SizeError = MakeScreenshotTooLargeMessageForMcp(static_cast<int32>(PngData.Num()));
    Resp->SetBoolField(TEXT("success"), false);
    Resp->SetStringField(TEXT("error"), SizeError);
    Resp->SetStringField(TEXT("message"), SizeError);
    SendAutomationResponse(Socket, RequestId, false, SizeError, Resp,
                           TEXT("IMAGE_TOO_LARGE"));
    return true;
  }
  if (bReturnBase64) {
    Resp->SetStringField(TEXT("imageBase64"),
                         FBase64::Encode(PngData.GetData(), static_cast<uint32>(PngData.Num())));
  }
  Resp->SetStringField(TEXT("message"),
      bReturnBase64
          ? TEXT("Screenshot captured and returned as image/png base64.")
          : TEXT("Screenshot captured."));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Screenshot captured"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Screenshot requires editor build."), nullptr);
  return true;
#endif
}
