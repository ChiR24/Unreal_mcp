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
// Last-resort window for full_editor_window when the root lookup comes back
// unusable (notably while PIE holds focus).
TSharedPtr<SWindow> GetAnyVisibleEditorWindowForMcp();

// Every visible editor window, main frame and floating asset editors alike, in
// the order the `window` selector indexes them. Minimized windows are included:
// the editor minimizes itself on launch and after some PIE cycles, and leaving
// it out of the list made the main frame unaddressable by index OR title, so
// full_editor_window answered EDITOR_WINDOW_NOT_FOUND with an empty window list
// and there was no in-tool way to get the capture back.
void EnumerateEditorSlateWindowsForMcp(TArray<TSharedRef<SWindow>> &OutWindows);
// Brings a minimized window back on screen so it can be photographed, WITHOUT
// activating it: SWindow::Restore() routes to SW_RESTORE and would steal the
// user's focus and cursor. Returns true when it had to un-minimize.
bool RestoreWindowForCaptureForMcp(const TSharedRef<SWindow> &Window);
void AppendEditorWindowListForMcp(const TSharedPtr<FJsonObject> &Resp);
// Resolves a window by list index or case-insensitive title substring.
TSharedPtr<SWindow> FindEditorSlateWindowForMcp(const FString &Query,
                                                FString &OutResolvedTitle,
                                                FString &OutError);

bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError);
#endif
