#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EditorViewportClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Exporters/Exporter.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorSubsystem.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Slate/SceneViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "UnrealClient.h"
#include "Foundation/McpScreenshotResample.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Widgets/SWindow.h"

// The viewport, editor-window and capture primitives the screenshot, inspect, perception, play, audio and timed-run
// handlers share. ControlEditor's handlers drive them; none of them is a handler.
FEditorViewportClient *GetActiveEditorViewportClientForMcp();
// The editor viewport that draws the play world while the player is ejected from its pawn (Simulate in Editor);
// null when Play In Editor is not running or the player still possesses a pawn.
FEditorViewportClient *GetEjectedPieViewportClientForMcp();
// Play In Editor runs and the player is still in its pawn: the game draws that pawn's camera, so no editor viewport
// camera can be moved. Sends the refusal (PIE_VIEW_NOT_EJECTED, naming the fix) and returns true; false otherwise.
bool RefuseCameraMoveWhilePieFollowsPawnForMcp(UEngineWireSubsystem *Bridge,
                                               TSharedPtr<FMcpBridgeWebSocket> Socket,
                                               const FString &RequestId, const TCHAR *What);

constexpr int32 MaxScreenshotPngBytesForBase64ForMcp = 3 * 1024 * 1024;

FString MakeSafeScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload);
void AddScreenshotMetadataForMcp(const TSharedPtr<FJsonObject> &Resp,
                                 const TSharedPtr<FJsonObject> &Payload);
FString MakeScreenshotTooLargeMessageForMcp(int32 SizeBytes);
// Saves the PNG and sends the receipt tail both capture paths share: path,
// size, opt-in base64, and the SAVE_FAILED / IMAGE_TOO_LARGE refusals. `What`
// names the capture in the messages ("Screenshot", "Full editor window screenshot").
void SendScreenshotReceiptForMcp(UEngineWireSubsystem *Subsystem,
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
// -McpStartMinimized: puts the main window away once startup has shown it and keeps it down for 10 s, without taking
// focus. Does nothing without the switch; a restore made over the bridge ends the hold.
void HoldEditorMinimizedAtStartupForMcp();
// What a timed run (sample_motion) holds while it runs: the main frame restored if it was minimized, and the
// Use Less CPU when in Background preference off in memory only (never saved), because a background editor
// steps PIE at about 3 fps with it on. EndEditorRunForMcp minimizes the frame again, unless a restore was asked
// for while the run went on, and puts the preference back.
// ponytail: with two runs overlapping, the first to end puts the preference back and the other may be throttled
// again for the rest of its run; a shared count if that ever matters.
struct FMcpEditorRunHold {
  bool bWindowRestored = false;
  bool bThrottleWasOn = false;
  uint32 RestoreCount = 0;
};
FMcpEditorRunHold BeginEditorRunForMcp();
// A configure_editor request to restore the window, counted even when the window is up already: the startup hold and
// the end of a timed run then leave the window as the caller asked.
void CountWindowRestoreRequestForMcp();
void EndEditorRunForMcp(const FMcpEditorRunHold &Hold);
void AppendEditorWindowListForMcp(const TSharedPtr<FJsonObject> &Resp);
// Draws the viewport Frames times, each after World sends its pending render updates, so a component edited or a
// particle system advanced since the last frame shows in the picture (it drew the old state). World may be null.
void DrawViewportFramesForMcp(FViewport *Viewport, UWorld *World, int32 Frames);
// A capture's location, rotation and fov (degrees across) put the level viewport's camera there first, where it stays;
// the field of view resets when the editor restarts, so a framing check names it. True when any was given: the old
// view's occlusion lingers for a few frames, so the caller draws a few before reading.
bool PlaceViewCameraForMcp(FEditorViewportClient *Client, const TSharedPtr<FJsonObject> &Payload);
// screenshot camera: writes the view of the camera actor it names (as it is now, a paused sequence's keys included)
// into Payload as location, rotation and fov, plus renderSize at the camera's own frame shape when it keeps one and
// none was asked for, so the capture shows that camera's shot. True when it refused instead (reply sent): a mode other
// than editor_viewport, location/rotation/fov given too, no such actor, or no perspective camera on it.
bool RefuseOrTakeCameraViewForMcp(UEngineWireSubsystem *Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                  const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, const FString &Mode);
// Its core for a capture that sends its own refusals (McpCaptureView: describe_view, analyze_view, audit_materials):
// true when Payload names no camera or the camera's view was taken into it; false with OutCode and OutError when not.
// bLevelViewport false refuses any camera (a capture that is not of the level viewport).
bool TakeCameraViewForMcp(const TSharedPtr<FJsonObject> &Payload, bool bLevelViewport, FString &OutCode, FString &OutError);
// A shot taken through a camera (Payload marked by RefuseOrTakeCameraViewForMcp) is drawn as a player sees it: the
// viewport is in game view (no editor icons, camera meshes or selection) while this lives, and as it was after.
struct FMcpCameraGameViewScope {
  FEditorViewportClient *Client = nullptr;
  bool bWasGameView = false;
  FMcpCameraGameViewScope(FEditorViewportClient *InClient, const TSharedPtr<FJsonObject> &Payload);
  ~FMcpCameraGameViewScope();
};
// True when the frame about to be read matches the one Payload recorded on the capture's previous pass (Camera/
// McpViewSettle.cpp); false, after recording this one, when the capture should be taken again a beat later, so
// streaming that lands over engine ticks (Nanite pages, texture mips) is in the picture. At most five passes.
bool ViewFrameSettledForMcp(FViewport *Viewport, const TSharedPtr<FJsonObject> &Payload);

// What a level viewport capture cannot show as it is: a viewport that is not realtime, video memory over budget.
TArray<TSharedPtr<FJsonValue>> ViewCaptureWarningsForMcp(const FEditorViewportClient *Client);
// Camera/McpViewRender.cpp, screenshot renderSize ("WxH"): sends the refusal of a renderSize the capture cannot honour
// (malformed, a window capture, a player in the game) and returns true when it did.
bool RefuseRenderSizeForMcp(UEngineWireSubsystem *Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                            const TSharedPtr<FJsonObject> &Payload, const FString &Mode, bool bEjectedView);
// The frame a viewport capture returns: the viewport's own pixels, or with renderSize its view drawn off-screen at that
// size, alpha opaque; InOutSize comes back as the frame's size. False with a code and message when no frame came back.
bool ReadViewFrameForMcp(FViewport *Viewport, const TSharedPtr<FJsonObject> &Payload, TArray<FColor> &OutPixels,
                         FIntPoint &InOutSize, FString &OutCode, FString &OutError);
// Resolves a window by list index or case-insensitive title substring.
TSharedPtr<SWindow> FindEditorSlateWindowForMcp(const FString &Query,
                                                FString &OutResolvedTitle,
                                                FString &OutError);

// A viewport frame made into the picture a capture returns: resampled to the payload's resolution when that asks for
// another size (a frame shorter than FrameSize is left as it is), then PNG-encoded at OutSize; OutPng is empty when the
// encoder made nothing. False, with the frame encoded at its own size, when resolution is not one (OutError says why).
bool EncodeViewShotForMcp(const TSharedPtr<FJsonObject> &Payload, TArray<FColor> &Bitmap, FIntPoint FrameSize,
                          FIntPoint &OutSize, TArray64<uint8> &OutPng, FString &OutError);
// width and height of the picture, and viewportWidth and viewportHeight (the frame's) when it was resampled.
void AddViewShotSizeForMcp(const TSharedPtr<FJsonObject> &Resp, FIntPoint Size, FIntPoint FrameSize);

// OutSize is the PNG's size; OutSourceSize, when given, the window's before any downscale.
bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize, FString &OutError,
                                 FIntPoint *OutSourceSize = nullptr);
