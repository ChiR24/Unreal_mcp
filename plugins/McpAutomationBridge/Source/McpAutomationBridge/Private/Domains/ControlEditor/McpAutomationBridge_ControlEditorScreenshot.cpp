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
    // The UI handler gates on the payload's own subAction, which still carries
    // whichever ALIAS the caller used. `take_screenshot` therefore fell past
    // the screenshot branch and answered "System control action
    // 'take_screenshot' not implemented" for a mode this action publishes.
    // Forward under the canonical name; the alias is a routing detail.
    Payload->SetStringField(TEXT("subAction"), TEXT("screenshot"));
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
    // `window` selects any open editor window - the main frame is only the
    // default. An asset editor (Widget Blueprint designer, material graph) lives
    // in its own window, so without this it could never be photographed.
    FString WindowQuery;
    Payload->TryGetStringField(TEXT("window"), WindowQuery);
    if (WindowQuery.IsEmpty() && Payload->HasTypedField<EJson::Number>(TEXT("window"))) {
      WindowQuery = FString::FromInt(
          static_cast<int32>(Payload->GetNumberField(TEXT("window"))));
    }

    FString ResolvedWindowTitle;
    TSharedPtr<SWindow> EditorWindow;
    if (!WindowQuery.IsEmpty()) {
      FString FindError;
      EditorWindow = FindEditorSlateWindowForMcp(WindowQuery, ResolvedWindowTitle, FindError);
      if (!EditorWindow.IsValid()) {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        AppendEditorWindowListForMcp(Details);
        SendStandardErrorResponse(this, Socket, RequestId,
                                  TEXT("EDITOR_WINDOW_NOT_FOUND"), FindError, Details);
        return true;
      }
    } else {
      EditorWindow = GetFullEditorSlateWindowForMcp();
      if (!EditorWindow.IsValid()) {
        EditorWindow = GetAnyVisibleEditorWindowForMcp();
      }
      if (EditorWindow.IsValid()) {
        ResolvedWindowTitle = EditorWindow->GetTitle().ToString();
      }
    }
    if (!EditorWindow.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId,
                                TEXT("EDITOR_WINDOW_NOT_AVAILABLE"),
                                TEXT("No visible editor window available for full editor screenshot"),
                                nullptr);
      return true;
    }

    // The editor minimizes itself on launch and after some PIE cycles; a
    // minimized window sits at -32000,-32000 and photographs as nothing.
    const bool bRestored = RestoreWindowForCaptureForMcp(EditorWindow.ToSharedRef());

    TArray<uint8> PngData;
    FIntVector ImageSize(0, 0, 0);
    FString CaptureError;
    if (!CaptureSlateWindowPngForMcp(EditorWindow.ToSharedRef(), Payload,
                                     PngData, ImageSize, CaptureError)) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                                CaptureError, nullptr);
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetNumberField(TEXT("width"), ImageSize.X);
    Resp->SetNumberField(TEXT("height"), ImageSize.Y);
    // Report which window was photographed and what else was open, so the next
    // call can address a different one without guessing at titles.
    Resp->SetStringField(TEXT("window"), ResolvedWindowTitle);
    Resp->SetBoolField(TEXT("windowRestored"), bRestored);
    AppendEditorWindowListForMcp(Resp);
    SendScreenshotReceiptForMcp(this, Socket, RequestId, Payload, Resp,
                                PngData.GetData(), PngData.Num(), FullPath,
                                TEXT("Full editor window screenshot"));
    return true;
  }

  FViewport* Viewport = nullptr;
  FEditorViewportClient* CaptureClient = nullptr;
  if (GEditor->PlayWorld != nullptr && GEditor->GetPIEViewport() != nullptr) {
    Viewport = GEditor->GetPIEViewport();
  }
  if (!Viewport) {
    // A level viewport under another major tab is never painted and reads back
    // solid black. Bring the level editor forward and take the picture a few
    // frames later, once Slate has painted it, instead of returning black as a
    // success. The marker keeps the retry to one.
    if (!Payload->HasField(TEXT("_levelEditorFronted")) &&
        BringLevelEditorTabToFrontForMcp()) {
      Payload->SetBoolField(TEXT("_levelEditorFronted"), true);
      TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
      FTSTicker::GetCoreTicker().AddTicker(
          FTickerDelegate::CreateLambda([WeakThis, RequestId, Payload, Socket](float) {
            if (WeakThis.IsValid()) {
              WeakThis->HandleControlEditorScreenshot(RequestId, Payload, Socket);
            }
            return false;
          }),
          0.3f);
      return true;
    }
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

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("mode"), Mode);
  // width/height describe the PNG actually returned. When a resample happened
  // the untouched viewport size rides alongside, so a caller comparing the two
  // can tell a downscaled frame from a native-resolution one.
  Resp->SetNumberField(TEXT("width"), OutputSize.X);
  Resp->SetNumberField(TEXT("height"), OutputSize.Y);
  if (OutputSize != ViewportSize) {
    Resp->SetNumberField(TEXT("viewportWidth"), ViewportSize.X);
    Resp->SetNumberField(TEXT("viewportHeight"), ViewportSize.Y);
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
  // Say so when the capture had to switch tabs: the caller's editor now shows
  // the level editor where it showed something else.
  if (Payload->HasField(TEXT("_levelEditorFronted"))) {
    Resp->SetBoolField(TEXT("levelEditorBroughtToFront"), true);
  }
  SendScreenshotReceiptForMcp(this, Socket, RequestId, Payload, Resp,
                              PngData.GetData(), PngData.Num(), FullPath,
                              TEXT("Screenshot"));
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Screenshot requires editor build."), nullptr);
  return true;
#endif
}
