#pragma once

// MetaHuman Creator authoring, driven entirely by UObject reflection.
//
// The MetaHuman Creator plugin (Engine/Plugins/MetaHuman/MetaHumanCharacter)
// only exists on UE 5.6+, while this plugin advertises 5.0-5.8. Its modules
// also carry no _API export we could link against. Both problems disappear if
// the surface is reached reflectively: every entry point we need is a
// BlueprintCallable UFUNCTION on UMetaHumanCharacterEditorSubsystem, so
// FindFunctionByName + ProcessEvent reaches it with no link dependency, no
// MCP_HAS_* define and no engine-version branch. On an engine without MetaHuman
// the class lookup simply returns null and the handlers report the capability
// as unavailable.
//
// The same reasoning drives Fab in McpAutomationBridgeFab/McpFabDirectApi.cpp.

#include "CoreMinimal.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMcpMetaHumanHandlers, Log, All);

#if WITH_EDITOR
namespace McpMetaHumanHandlers
{
using FMetaHumanSocket = TSharedPtr<FMcpBridgeWebSocket>;

/** Object paths of the reflected MetaHuman types, resolved by name at runtime. */
extern const TCHAR* const MetaHumanCharacterClassPath;
extern const TCHAR* const MetaHumanSubsystemClassPath;
extern const TCHAR* const MetaHumanFactoryClassPath;

/** The UMetaHumanCharacter asset class, or nullptr when MetaHuman is absent. */
UClass* FindMetaHumanCharacterClass();

/** The live UMetaHumanCharacterEditorSubsystem, or nullptr when absent. */
UObject* FindMetaHumanSubsystem();

/**
 * Whether "MetaHuman Creator Core Data" is installed next to the engine.
 *
 * Mirrors FMetaHumanCharacterEditorModule::IsOptionalMetaHumanContentInstalled,
 * which is a non-exported static in a module we deliberately do not link, so
 * the folder probe is reproduced here from the plugin's own content dir. Without
 * this content MetaHuman loads with "limited features": texture synthesis and
 * body textures are unavailable, and a build produces an untextured character.
 */
bool IsCoreDataInstalled(FString& OutDetail);

/**
 * Call a reflected UFUNCTION on Target, binding Args onto its parameters.
 *
 * OutResults receives the function's out and return parameters. Returns false
 * with OutError set when the function does not exist on this engine's MetaHuman
 * version or an argument does not convert -- never silently no-ops, because a
 * missing UFUNCTION is exactly how an older MetaHuman presents itself.
 */
bool InvokeMetaHumanFunction(UObject* Target, const TCHAR* FunctionName,
    const TSharedPtr<FJsonObject>& Args, UObject* CharacterArg,
    TSharedPtr<FJsonObject>& OutResults, FString& OutError);

/** Load a MetaHuman character asset by package path, or nullptr. */
UObject* LoadMetaHumanCharacter(const FString& AssetPath);

/**
 * Resolve the character and register it for editing.
 *
 * Most subsystem functions require TryAddObjectToEdit first and fail with only
 * a log line otherwise, so every mutating handler funnels through this and
 * answers a typed error instead.
 */
UObject* RequireEditableCharacter(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket, UObject*& OutSubsystem);

/** Answer a typed error when MetaHuman is not present on this engine. */
void SendMetaHumanUnavailable(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FMetaHumanSocket Socket);

bool HandleMetaHumanStatus(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket);
bool HandleCreateMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket);
bool HandleRigMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket);
bool HandleBuildMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket);
bool HandleExportMetaHuman(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FMetaHumanSocket Socket);
}
#endif
