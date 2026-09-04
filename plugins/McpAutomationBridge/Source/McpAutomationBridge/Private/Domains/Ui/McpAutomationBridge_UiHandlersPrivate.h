#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Templates/Function.h"

// Grants the UI/system-control adapters access to the subsystem's private
// action handlers (console, audio, debug), mirroring FMcpLevelHandlerAccess.
struct FMcpUiHandlerAccess {
  static bool ConsoleCommand(UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId,
                             const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket) {
    return Subsystem.HandleConsoleCommandAction(RequestId, TEXT("console_command"), Payload, Socket);
  }
  static bool Audio(UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId, const FString &Action,
                    const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket) {
    return Subsystem.HandleAudioAction(RequestId, Action, Payload, Socket);
  }
  static bool Debug(UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId, const FString &Action,
                    const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket) {
    return Subsystem.HandleDebugAction(RequestId, Action, Payload, Socket);
  }
};

#if WITH_EDITOR
namespace McpUiHandlers {

using FUiScreenshotFallback = TFunction<bool()>;

bool HandleWidgetAuthoringAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode);

bool HandleScreenshotAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    bool bIsSystemControl, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode, bool &bResponseSent,
    const FUiScreenshotFallback &ScreenshotFallback);

bool HandleEditorControlAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode);

bool HandleRuntimeWidgetAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode);

bool HandleProjectSettingsAction(const FString &LowerSub,
                                 const TSharedPtr<FJsonObject> &Payload,
                                 const TSharedPtr<FJsonObject> &Resp,
                                 bool &bSuccess, FString &Message,
                                 FString &ErrorCode);
// lumen_update_scene, play_sound, profile, set_cvar, set_fullscreen,
// set_resolution, show_widget, spawn_category. Sends its own response and
// returns true when it handled the action.
bool HandleSystemExtrasAction(UMcpAutomationBridgeSubsystem &Bridge,
                              const FString &RequestId, const FString &LowerSub,
                              const TSharedPtr<FJsonObject> &Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket);

}
#endif
