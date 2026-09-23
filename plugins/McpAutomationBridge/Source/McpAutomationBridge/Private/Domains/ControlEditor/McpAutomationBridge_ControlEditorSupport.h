#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
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

#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif
#if ENGINE_MAJOR_VERSION > 5 || \
    (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#define MCP_CONTROL_HAS_INPUT_DEVICE_ID 1
#else
#define MCP_CONTROL_HAS_INPUT_DEVICE_ID 0
#endif
#if ENGINE_MAJOR_VERSION > 5 || \
    (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS 1
#else
#define MCP_CONTROL_HAS_SIMULATED_INPUT_EVENT_ARGS 0
#endif

bool IsSafeConsoleArgumentToken(const FString &Value);
FString MakeSafeConsoleName(const FString &RawName, const TCHAR *Prefix);
FEditorViewportClient *GetActiveEditorViewportClientForMcp();
TSharedPtr<FJsonObject> MakeVectorObjectForMcp(const FVector &Vector);
TSharedPtr<FJsonObject> MakeRotatorObjectForMcp(const FRotator &Rotator);
AActor *FindActorByNameInWorldForMcp(UWorld *World, const FString &Target,
                                     bool bExactMatchOnly);
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
// Drop every live Enhanced Input hold. The holds run on the core ticker, which
// outlives this module, so a delegate still registered when the module unloads
// would call into code that is no longer there -- and Live Coding unloads this
// module routinely. Call from subsystem shutdown.
void StopAllEnhancedInputHoldsForMcp();
#endif
