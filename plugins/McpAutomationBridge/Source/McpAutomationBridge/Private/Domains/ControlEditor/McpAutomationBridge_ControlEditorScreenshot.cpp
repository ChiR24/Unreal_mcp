#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // waitForShaders: capture once the shader queue drains. The wait polls on the core ticker, so the game thread
  // keeps running and the compiling can finish; the call resumes here with shaderWait in the payload.
  if (McpDeferForShaderCompile(
          Payload, [Weak = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(this), RequestId,
                    Socket](const TSharedPtr<FJsonObject> &Waited) {
            if (UMcpAutomationBridgeSubsystem *Self = Weak.Get()) {
              Self->HandleControlEditorScreenshot(RequestId, Waited, Socket);
            }
          })) {
    return true;
  }

  // Takes the picture again a few frames later, once Slate has painted what this pass brought forward.
  auto RetryLater = [this, &RequestId, &Payload, &Socket]() {
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([WeakThis, RequestId, Payload, Socket](float) {
          if (WeakThis.IsValid()) {
            WeakThis->HandleControlEditorScreenshot(RequestId, Payload, Socket);
          }
          return false;
        }),
        0.3f);
  };

  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  Mode = Mode.TrimStartAndEnd().ToLower();
  if (Mode.IsEmpty()) {
    Mode = TEXT("editor_viewport");
  }

  // location/rotation place the camera the picture is taken from. While Play In Editor runs that is only possible once
  // the player is ejected; a pawn's camera cannot be moved, and a picture from somewhere else is not what was asked for.
  if ((Mode == TEXT("editor_viewport") || Mode == TEXT("game_viewport")) &&
      (Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation"))) &&
      RefuseCameraMoveWhilePieFollowsPawnForMcp(this, Socket, RequestId,
                                                TEXT("screenshot with location or rotation"))) {
    return true;
  }

  // An ejected player no longer feeds the game viewport: the picture of the game is the editor viewport that draws it.
  const bool bEjectedView = GetEjectedPieViewportClientForMcp() != nullptr;
  if (Mode == TEXT("game_viewport") && !bEjectedView) {
    // The UI handler gates on the payload's own subAction, which still carries
    // whichever ALIAS the caller used. `take_screenshot` therefore fell past
    // the screenshot branch and answered "System control action
    // 'take_screenshot' not implemented" for a mode this action publishes.
    // Forward under the canonical name; the alias is a routing detail.
    Payload->SetStringField(TEXT("subAction"), TEXT("screenshot"));
    return HandleUiAction(RequestId, TEXT("system_control"), Payload, Socket);
  }

  // game_viewport gets here only for an ejected player, and is then the same picture as editor_viewport.
  if (Mode != TEXT("editor_viewport") && Mode != TEXT("full_editor_window") && Mode != TEXT("game_viewport")) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        TEXT("Invalid screenshot mode. Supported modes: editor_viewport, game_viewport, full_editor_window"),
        nullptr);
    return true;
  }

  const FString Filename = MakeSafeScreenshotFilenameForMcp(Payload);

  // path was declared for every mode but honoured only by game_viewport.
  FString ScreenshotDir;
  FString PathError;
  if (!ResolveScreenshotDirectoryForMcp(Payload, FPaths::ProjectSavedDir() / TEXT("Screenshots"),
                                        ScreenshotDir, PathError)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"), PathError, nullptr);
    return true;
  }
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
      // Always the main editor frame. Falling back to "whichever window is
      // usable" photographed the Message Log while the main frame was minimized.
      if (FSlateApplication::IsInitialized()) {
        EditorWindow = FGlobalTabmanager::Get()->GetRootWindow();
      }
      if (EditorWindow.IsValid()) {
        ResolvedWindowTitle = EditorWindow->GetTitle().ToString();
      }
    }
    if (!EditorWindow.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId,
                                TEXT("EDITOR_WINDOW_NOT_AVAILABLE"),
                                TEXT("The main editor window is not open, so there is no editor window to capture"),
                                nullptr);
      return true;
    }

    // The editor minimizes itself on launch and after some PIE cycles; a
    // minimized window sits off screen and photographs as nothing. It is put
    // back WITHOUT activating it (no focus change, no cursor move); when that
    // cannot be done the capture is refused rather than taken of another window.
    // A window just restored still shows the viewport frame drawn before it was minimized, so a camera moved
    // since then was not in the picture: redraw the viewports and take it a few frames later.
    const bool bRestoredNow = RestoreWindowForCaptureForMcp(EditorWindow.ToSharedRef());
    if (bRestoredNow && !Payload->HasField(TEXT("_windowRestoredForCapture"))) {
      Payload->SetBoolField(TEXT("_windowRestoredForCapture"), true);
      GEditor->RedrawAllViewports(true);
      RetryLater();
      return true;
    }
    const bool bRestored = bRestoredNow || Payload->HasField(TEXT("_windowRestoredForCapture"));
    if (EditorWindow->IsWindowMinimized()) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_WINDOW_MINIMIZED"),
                                FString::Printf(TEXT("The editor window '%s' is minimized and could not be restored "
                                                     "without taking focus; restore it and retry"),
                                                *ResolvedWindowTitle),
                                nullptr);
      return true;
    }

    TArray<uint8> PngData;
    FIntVector ImageSize(0, 0, 0);
    FIntPoint SourceSize(0, 0);
    FString CaptureError;
    const bool bCaptured = CaptureSlateWindowPngForMcp(EditorWindow.ToSharedRef(), Payload,
                                                       PngData, ImageSize, CaptureError, &SourceSize);
    // A window that had to be restored for the picture goes back the way it was found,
    // whether or not the picture was taken: minimized, still without taking focus.
    if (bRestored) {
      MinimizeWindowForMcp(EditorWindow.ToSharedRef());
    }
    if (!bCaptured) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                                CaptureError, nullptr);
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetNumberField(TEXT("width"), ImageSize.X);
    Resp->SetNumberField(TEXT("height"), ImageSize.Y);
    // As for the viewport: a downscaled window names the size it was taken at.
    if (SourceSize.X != ImageSize.X || SourceSize.Y != ImageSize.Y) {
      Resp->SetNumberField(TEXT("viewportWidth"), SourceSize.X);
      Resp->SetNumberField(TEXT("viewportHeight"), SourceSize.Y);
    }
    // Report which window was photographed and what else was open, so the next
    // call can address a different one without guessing at titles.
    Resp->SetStringField(TEXT("window"), ResolvedWindowTitle);
    Resp->SetBoolField(TEXT("mainWindow"), EditorWindow == FGlobalTabmanager::Get()->GetRootWindow());
    Resp->SetBoolField(TEXT("windowRestored"), bRestored);
    AppendEditorWindowListForMcp(Resp);
    SendScreenshotReceiptForMcp(this, Socket, RequestId, Payload, Resp,
                                PngData.GetData(), PngData.Num(), FullPath,
                                TEXT("Full editor window screenshot"));
    return true;
  }

  FViewport* Viewport = nullptr;
  FEditorViewportClient* CaptureClient = nullptr;
  if (GEditor->PlayWorld != nullptr && !bEjectedView && GEditor->GetPIEViewport() != nullptr) {
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
      RetryLater();
      return true;
    }
    // Resolve through the same helper the camera handlers use, so the viewport
    // that gets moved is provably the viewport that gets photographed.
    CaptureClient = GetActiveEditorViewportClientForMcp();
    if (CaptureClient) {
      // location/rotation place the camera and take the picture in one call; set_camera and then a
      // screenshot could photograph a frame drawn before the move.
      CaptureClient->SetViewLocation(ExtractVectorField(Payload, TEXT("location"), CaptureClient->GetViewLocation()));
      CaptureClient->SetViewRotation(ExtractRotatorField(Payload, TEXT("rotation"), CaptureClient->GetViewRotation()));
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

  // The first frames after a camera move still use the occlusion results of the old view, which hide small things
  // (a sign's text, a tree) the new view shows; a few frames later the picture is what the camera sees.
  const bool bCameraMoved = CaptureClient && (Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation")));
  for (int32 Frame = bCameraMoved ? 0 : 2; Frame < 3; ++Frame) {
    Viewport->Draw();
    FlushRenderingCommands();
  }

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
  // Which view the picture is of: the level viewport, the game as its pawn sees it, or the free camera of an
  // ejected player. mode, the one asked for, is named only when the picture is of another.
  const TCHAR *View = GEditor->PlayWorld == nullptr ? TEXT("editor_viewport")
                      : bEjectedView                ? TEXT("pie_ejected")
                                                    : TEXT("pie_game");
  Resp->SetStringField(TEXT("view"), View);
  if (!Mode.Equals(View, ESearchCase::CaseSensitive)) {
    Resp->SetStringField(TEXT("mode"), Mode);
  }
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
                         McpHandlerUtils::VectorToJson(CaptureClient->GetViewLocation()));
    Resp->SetObjectField(TEXT("cameraRotation"),
                         McpHandlerUtils::RotatorToJson(CaptureClient->GetViewRotation()));
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
}
