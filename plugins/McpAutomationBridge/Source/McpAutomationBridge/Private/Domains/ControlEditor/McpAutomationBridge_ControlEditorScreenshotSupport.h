#pragma once

#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Foundation/McpScreenshotResample.h"

#if WITH_EDITOR
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Widgets/SWindow.h"

constexpr int32 MaxScreenshotPngBytesForBase64ForMcp = 3 * 1024 * 1024;

FString MakeSafeScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload);
void AddScreenshotMetadataForMcp(const TSharedPtr<FJsonObject> &Resp,
                                 const TSharedPtr<FJsonObject> &Payload);
FString MakeScreenshotTooLargeMessageForMcp(int32 SizeBytes);

// ResolveScreenshotResolutionForMcp / ResampleBitmapForMcp come from
// Foundation/McpScreenshotResample.h so all three capture surfaces share one
// implementation.

TSharedPtr<SWindow> GetFullEditorSlateWindowForMcp();
bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError);
#endif
