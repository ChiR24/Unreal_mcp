#pragma once

#include "McpAutomationBridge_ControlEditorSupport.h"

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
TSharedPtr<SWindow> GetFullEditorSlateWindowForMcp();
bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError);
#endif
