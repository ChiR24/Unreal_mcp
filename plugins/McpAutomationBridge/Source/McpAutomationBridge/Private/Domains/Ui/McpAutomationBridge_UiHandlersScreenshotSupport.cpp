#include "Domains/Ui/McpAutomationBridge_UiHandlersScreenshotSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/Paths.h"

namespace McpUiHandlers {

FString MakeSafeUiScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload) {
  FString Filename;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("filename"), Filename);
  }

  if (Filename.IsEmpty()) {
    Filename = FString::Printf(TEXT("Screenshot_%lld"),
                               FDateTime::Now().ToUnixTimestamp());
  }

  Filename = FPaths::GetCleanFilename(Filename);
  if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) ||
      Filename.Contains(TEXT("\\"))) {
    Filename = FString::Printf(TEXT("Screenshot_%lld"),
                               FDateTime::Now().ToUnixTimestamp());
  }

  if (!Filename.EndsWith(TEXT(".png"))) {
    Filename += TEXT(".png");
  }

  return Filename;
}

void AddScreenshotMetadataForUiMcp(const TSharedPtr<FJsonObject> &Resp,
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

FString MakeScreenshotTooLargeMessageForUiMcp(int32 SizeBytes) {
  return FString::Printf(
      TEXT("Screenshot PNG is too large to return as base64 (%d bytes, max %d bytes). Retry with returnBase64=false or a smaller viewport/window."),
      SizeBytes, MaxScreenshotPngBytesForBase64ForMcp);
}

void AddViewportDetailsForUiScreenshot(
    const TSharedPtr<FJsonObject> &Resp, UGameViewportClient *ViewportClient,
    const FString &ActiveViewTargetPath, bool bHasPlayerCamera,
    const FVector &ActiveCameraLocation, const FRotator &ActiveCameraRotation,
    float ActiveCameraFov) {
  if (UWorld *ViewportWorld = ViewportClient->GetWorld()) {
    Resp->SetStringField(TEXT("viewportWorld"), ViewportWorld->GetName());
    Resp->SetNumberField(TEXT("viewportWorldType"),
                         static_cast<int32>(ViewportWorld->WorldType));
  }
  if (!ActiveViewTargetPath.IsEmpty()) {
    Resp->SetStringField(TEXT("activeViewTarget"), ActiveViewTargetPath);
  }
  if (!bHasPlayerCamera) {
    return;
  }

  Resp->SetObjectField(TEXT("activeCameraLocation"), McpHandlerUtils::VectorToJson(ActiveCameraLocation));

  Resp->SetObjectField(TEXT("activeCameraRotation"), McpHandlerUtils::RotatorToJson(ActiveCameraRotation));
  Resp->SetNumberField(TEXT("activeCameraFov"), ActiveCameraFov);
}

}
