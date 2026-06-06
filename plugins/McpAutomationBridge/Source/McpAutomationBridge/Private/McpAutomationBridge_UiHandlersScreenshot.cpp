#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_UiHandlersPrivate.h"

#include "Camera/PlayerCameraManager.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridge_UiHandlersScreenshotSupport.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "UnrealClient.h"

#if WITH_EDITOR
namespace McpUiHandlers {

bool HandleScreenshotAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    bool bIsSystemControl, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode, bool &bResponseSent,
    const FUiScreenshotFallback &ScreenshotFallback) {
  if (LowerSub != TEXT("screenshot")) {
    return false;
  }

  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  Mode = Mode.TrimStartAndEnd().ToLower();
  if (Mode.IsEmpty() && bIsSystemControl) {
    bResponseSent = true;
    return ScreenshotFallback();
  }
  if (Mode == TEXT("full_editor_window") || Mode == TEXT("editor_viewport")) {
    bResponseSent = true;
    return ScreenshotFallback();
  }
  if (!Mode.IsEmpty() && Mode != TEXT("game_viewport")) {
    Message =
        TEXT("Invalid screenshot mode. Supported modes: editor_viewport, game_viewport, full_editor_window");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                                  Resp, ErrorCode);
    bResponseSent = true;
    return true;
  }

  FString RawScreenshotPath;
  Payload->TryGetStringField(TEXT("path"), RawScreenshotPath);

  FString ScreenshotPath;
  if (RawScreenshotPath.IsEmpty()) {
    ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor");
  } else {
    const FString SafePath = SanitizeProjectFilePath(RawScreenshotPath);
    if (SafePath.IsEmpty()) {
      Message = FString::Printf(
          TEXT("Invalid or unsafe screenshot path: %s. Path must be relative to project."),
          *RawScreenshotPath);
      ErrorCode = TEXT("SECURITY_VIOLATION");
      Resp->SetStringField(TEXT("error"), Message);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    Message, Resp, ErrorCode);
      bResponseSent = true;
      return true;
    }

    ScreenshotPath = FPaths::ProjectDir() / SafePath;
    ScreenshotPath = FPaths::ConvertRelativePathToFull(ScreenshotPath);
    FPaths::NormalizeFilename(ScreenshotPath);

    FString NormalizedProjectDir =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    if (!ScreenshotPath.StartsWith(NormalizedProjectDir,
                                   ESearchCase::IgnoreCase)) {
      Message = FString::Printf(
          TEXT("Invalid or unsafe screenshot path: %s. Path escapes project directory."),
          *RawScreenshotPath);
      ErrorCode = TEXT("SECURITY_VIOLATION");
      Resp->SetStringField(TEXT("error"), Message);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    Message, Resp, ErrorCode);
      bResponseSent = true;
      return true;
    }
  }

  const FString Filename = MakeSafeUiScreenshotFilenameForMcp(Payload);

  bool bReturnBase64 = true;
  Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

  UGameViewportClient *ViewportClient = nullptr;
  bool bUsingPieViewport = false;
  if (GEditor && GEditor->PlayWorld) {
    if (UWorld *PlayWorld = GEditor->PlayWorld.Get()) {
      ViewportClient = PlayWorld->GetGameViewport();
      bUsingPieViewport = ViewportClient != nullptr;
    }
  }
  if (!ViewportClient && GEngine) {
    ViewportClient = GEngine->GameViewport;
  }

  if (!ViewportClient) {
    Message = TEXT("No game viewport available");
    ErrorCode = TEXT("NO_VIEWPORT");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  FViewport *Viewport = ViewportClient->Viewport;
  if (!Viewport) {
    Message = TEXT("No viewport available");
    ErrorCode = TEXT("NO_VIEWPORT");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  bool bForcedViewportDraw = false;
  bool bHasPlayerCamera = false;
  FString ActiveViewTargetPath;
  FVector ActiveCameraLocation = FVector::ZeroVector;
  FRotator ActiveCameraRotation = FRotator::ZeroRotator;
  float ActiveCameraFov = 0.0f;

  if (UWorld *ViewportWorld = ViewportClient->GetWorld()) {
    if (APlayerController *PlayerController =
            ViewportWorld->GetFirstPlayerController()) {
      if (AActor *ViewTarget = PlayerController->GetViewTarget()) {
        ActiveViewTargetPath = ViewTarget->GetPathName();
      }
      if (APlayerCameraManager *CameraManager =
              PlayerController->PlayerCameraManager) {
        CameraManager->UpdateCamera(0.0f);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
        const FMinimalViewInfo &CameraView = CameraManager->GetCameraCacheView();
#else
        const FMinimalViewInfo CameraView = CameraManager->GetCameraCachePOV();
#endif
        ActiveCameraLocation = CameraView.Location;
        ActiveCameraRotation = CameraView.Rotation;
        ActiveCameraFov = CameraView.FOV;
        bHasPlayerCamera = true;
      }
    }
  }

  if (bUsingPieViewport) {
    Viewport->Draw(false);
    FlushRenderingCommands();
    bForcedViewportDraw = true;
  }

  TArray<FColor> Bitmap;
  const FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);
  const bool bReadSuccess = Viewport->ReadPixels(Bitmap);
  if (!bReadSuccess || Bitmap.Num() == 0) {
    Message = TEXT("Failed to read viewport pixels");
    ErrorCode = TEXT("CAPTURE_FAILED");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  const int32 Width = Size.X;
  const int32 Height = Size.Y;
  TArray<uint8> PngData;
  IImageWrapperModule &ImageWrapperModule =
      FModuleManager::LoadModuleChecked<IImageWrapperModule>(
          FName("ImageWrapper"));
  TSharedPtr<IImageWrapper> ImageWrapper =
      ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

  if (ImageWrapper.IsValid()) {
    TArray<uint8> RawData;
    RawData.SetNumUninitialized(Width * Height * 4);
    for (int32 i = 0; i < Bitmap.Num(); ++i) {
      RawData[i * 4 + 0] = Bitmap[i].R;
      RawData[i * 4 + 1] = Bitmap[i].G;
      RawData[i * 4 + 2] = Bitmap[i].B;
      RawData[i * 4 + 3] = 255;
    }

    if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width, Height,
                             ERGBFormat::RGBA, 8)) {
      PngData = ImageWrapper->GetCompressed(100);
    }
  }

  if (PngData.Num() == 0) {
    Message = TEXT("Failed to encode viewport screenshot as PNG");
    ErrorCode = TEXT("CAPTURE_FAILED");
    Resp->SetStringField(TEXT("error"), Message);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Message,
                                  Resp, ErrorCode);
    bResponseSent = true;
    return true;
  }

  FString FullPath = FPaths::Combine(ScreenshotPath, Filename);
  FPaths::MakeStandardFilename(FullPath);

  IFileManager::Get().MakeDirectory(*ScreenshotPath, true);
  const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

  bSuccess = true;
  Message = FString::Printf(TEXT("Screenshot captured (%dx%d)"), Width, Height);
  Resp->SetStringField(TEXT("screenshotPath"), FullPath);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("mode"), TEXT("game_viewport"));
  Resp->SetBoolField(TEXT("usingPieViewport"), bUsingPieViewport);
  Resp->SetBoolField(TEXT("forcedViewportDraw"), bForcedViewportDraw);
  AddViewportDetailsForUiScreenshot(Resp, ViewportClient, ActiveViewTargetPath,
                                    bHasPlayerCamera, ActiveCameraLocation,
                                    ActiveCameraRotation, ActiveCameraFov);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetNumberField(TEXT("width"), Width);
  Resp->SetNumberField(TEXT("height"), Height);
  Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());
  Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
  AddScreenshotMetadataForUiMcp(Resp, Payload);

  if (!bSaved && !bReturnBase64) {
    bSuccess = false;
    Message =
        TEXT("Screenshot captured but failed to save, and returnBase64=false leaves no image output.");
    ErrorCode = TEXT("SAVE_FAILED");
    Resp->SetStringField(TEXT("error"), Message);
  } else if (bReturnBase64 &&
             PngData.Num() > MaxScreenshotPngBytesForBase64ForMcp) {
    bSuccess = false;
    Message = MakeScreenshotTooLargeMessageForUiMcp(PngData.Num());
    ErrorCode = TEXT("IMAGE_TOO_LARGE");
    Resp->SetStringField(TEXT("error"), Message);
  }

  if (bSuccess && bReturnBase64) {
    Resp->SetStringField(TEXT("imageBase64"), FBase64::Encode(PngData));
  }
  return true;
}

}
#endif
