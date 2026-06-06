#include "McpAutomationBridge_ControlEditorScreenshotSupport.h"

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

  // Build the full path - save to project's Saved/Screenshots folder
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
    if (!CaptureSlateWindowPngForMcp(EditorWindow.ToSharedRef(), PngData,
                                     ImageSize, CaptureError)) {
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
  if (GEditor->PlayWorld != nullptr && GEditor->GetPIEViewport() != nullptr) {
    Viewport = GEditor->GetPIEViewport();
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

  TArray64<uint8> PngData;
  FImageUtils::PNGCompressImageArray(
      ViewportSize.X,
      ViewportSize.Y,
      TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()),
      PngData);
  if (PngData.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                              TEXT("Failed to encode viewport screenshot as PNG"), nullptr);
    return true;
  }

  const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

  bool bReturnBase64 = false;
  Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("mode"), Mode);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetNumberField(TEXT("width"), ViewportSize.X);
  Resp->SetNumberField(TEXT("height"), ViewportSize.Y);
  Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());
  Resp->SetNumberField(TEXT("fileSizeBytes"), PngData.Num());
  Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
  if (bSaved) {
    Resp->SetStringField(TEXT("path"), FullPath);
    Resp->SetStringField(TEXT("screenshotPath"), FullPath);
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
