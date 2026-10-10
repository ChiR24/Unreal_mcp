#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UGameViewportClient;

namespace McpUiHandlers {

constexpr int32 MaxScreenshotPngBytesForBase64ForMcp = 3 * 1024 * 1024;

FString MakeSafeUiScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload);

void AddScreenshotMetadataForUiMcp(const TSharedPtr<FJsonObject> &Resp,
                                   const TSharedPtr<FJsonObject> &Payload);

FString MakeScreenshotTooLargeMessageForUiMcp(int32 SizeBytes);

void AddViewportDetailsForUiScreenshot(
    const TSharedPtr<FJsonObject> &Resp, UGameViewportClient *ViewportClient,
    const FString &ActiveViewTargetPath, bool bHasPlayerCamera,
    const FVector &ActiveCameraLocation, const FRotator &ActiveCameraRotation,
    float ActiveCameraFov);

}
