#pragma once

#include "McpVersionCompatibility.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
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
#endif
