#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EditorViewportClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Exporters/Exporter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "IAssetViewport.h"
#include "Misc/OutputDevice.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "UnrealClient.h"

#include "FileHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "LevelEditorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "LevelEditor.h"
#include "Settings/LevelEditorPlaySettings.h"
#if ENGINE_MAJOR_VERSION > 5 ||  (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#define MCP_CONTROL_HAS_INPUT_DEVICE_ID 1
#else
#define MCP_CONTROL_HAS_INPUT_DEVICE_ID 0
#endif
#if ENGINE_MAJOR_VERSION > 5 ||  (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS 1
#else
#define MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS 0
#endif

bool IsSafeConsoleArgumentToken(const FString &Value);
FString MakeSafeConsoleName(const FString &RawName, const TCHAR *Prefix);
FEditorViewportClient *GetActiveEditorViewportClientForMcp();
// Sends a level-load reply once the level has settled (some frames, texture streaming quiet, a few seconds at most),
// with settledSeconds; bRestoreGameView puts back the game view the load switched off.
void McpReplyWhenLevelSettles(TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const FString &RequestId, const FString &Message, TSharedPtr<FJsonObject> Resp,
                              bool bRestoreGameView);
// The editor viewport that draws the play world while the player is ejected from its pawn (Simulate in Editor);
// null when Play In Editor is not running or the player still possesses a pawn.
FEditorViewportClient *GetEjectedPieViewportClientForMcp();
// Play In Editor runs and the player is still in its pawn: the game draws that pawn's camera, so no editor viewport
// camera can be moved. Sends the refusal (PIE_VIEW_NOT_EJECTED, naming the fix) and returns true; false otherwise.
bool RefuseCameraMoveWhilePieFollowsPawnForMcp(UMcpAutomationBridgeSubsystem *Bridge,
                                               TSharedPtr<FMcpBridgeWebSocket> Socket,
                                               const FString &RequestId, const TCHAR *What);
FString NormalizeSimulatedInputTypeForMcp(
    const TSharedPtr<FJsonObject> &Payload);
void SimulateEditorInputForMcp(const FString &InputType, const FString &Key,
                               const TSharedPtr<FJsonObject> &Payload,
                               bool &bSuccess, bool &bRoutedToPIE,
                               bool &bHandledByPIE, bool &bHandledBySlate,
                               FString &Message);
void AddSimulatedInputDiagnosticsForMcp(
    const FString &Key, const TSharedPtr<FJsonObject> &Resp);
// widget_list / widget_click: drive the PIE session's live UMG by reflection
// instead of the OS cursor. Fills Resp and returns whether the call succeeded.
bool SimulateLiveWidgetInputForMcp(const FString &InputType,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   const TSharedPtr<FJsonObject> &Resp,
                                   FString &Message);
// While a widget inside the PIE viewport holds keyboard focus, sends the key through Slate's focus path (as a real
// key arrives) and returns true with bOutHandled; returns false, sending nothing, otherwise.
bool RouteKeyToFocusedPieWidgetForMcp(const FKey &Key, EInputEvent InputEvent, bool &bOutHandled);
// Drop every live Enhanced Input hold. The holds run on the core ticker, which
// outlives this module, so a delegate still registered when the module unloads
// would call into code that is no longer there -- and Live Coding unloads this
// module routinely. Call from subsystem shutdown.
void StopAllEnhancedInputHoldsForMcp();
