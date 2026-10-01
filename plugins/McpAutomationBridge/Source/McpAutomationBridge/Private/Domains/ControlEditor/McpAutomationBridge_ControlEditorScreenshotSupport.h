#pragma once

#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Foundation/McpScreenshotResample.h"

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
// Saves the PNG and sends the receipt tail both capture paths share: path,
// size, opt-in base64, and the SAVE_FAILED / IMAGE_TOO_LARGE refusals. `What`
// names the capture in the messages ("Screenshot", "Full editor window screenshot").
void SendScreenshotReceiptForMcp(UMcpAutomationBridgeSubsystem *Subsystem,
                                 TSharedPtr<FMcpBridgeWebSocket> Socket,
                                 const FString &RequestId,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 const uint8 *PngData, int64 PngBytes,
                                 const FString &FullPath, const TCHAR *What);
// Brings the level editor's major tab to the front when another major tab
// (Fab, a docked asset editor) covers it. Returns true when it had to.
bool BringLevelEditorTabToFrontForMcp();

// ResolveScreenshotResolutionForMcp / ResampleBitmapForMcp come from
// Foundation/McpScreenshotResample.h so all three capture surfaces share one
// implementation.

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
// The other half: puts a window away again WITHOUT activating anything (a Win32 placement with
// SW_SHOWMINNOACTIVE; SWindow::Minimize() elsewhere), for a caller that had to restore it.
void MinimizeWindowForMcp(const TSharedRef<SWindow> &Window);
// What a timed run (sample_motion) holds while it runs: the main frame restored if it was minimized, and the
// Use Less CPU when in Background preference off in memory only (never saved), because a background editor
// steps PIE at about 3 fps with it on. EndEditorRunForMcp minimizes the frame again and puts the preference back.
// ponytail: with two runs overlapping, the first to end puts the preference back and the other may be throttled
// again for the rest of its run; a shared count if that ever matters.
struct FMcpEditorRunHold {
  bool bWindowRestored = false;
  bool bThrottleWasOn = false;
};
FMcpEditorRunHold BeginEditorRunForMcp();
void EndEditorRunForMcp(const FMcpEditorRunHold &Hold);
void AppendEditorWindowListForMcp(const TSharedPtr<FJsonObject> &Resp);
// Resolves a window by list index or case-insensitive title substring.
TSharedPtr<SWindow> FindEditorSlateWindowForMcp(const FString &Query,
                                                FString &OutResolvedTitle,
                                                FString &OutError);

// OutSize is the PNG's size; OutSourceSize, when given, the window's before any downscale.
bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError,
                                 FIntPoint *OutSourceSize = nullptr);
