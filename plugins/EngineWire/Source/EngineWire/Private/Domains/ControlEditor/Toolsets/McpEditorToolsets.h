// McpEditorToolsets.h — Epic's editor toolsets (the Toolset Registry that ships with UE 5.8) behind
// this plugin's gateway: listed, described and called with this plugin's scopes, consent and console policy.
//
// The registry is reached through reflection only (UToolsetRegistry's static UFUNCTIONs), so the plugin links nothing
// from an experimental plugin and loads unchanged on 5.0-5.7, where the class does not exist.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEngineWireSubsystem;
class FMcpBridgeWebSocket;

namespace McpEditorToolsets
{
// What a tool does: the access mode its schema states (annotations.accessMode, from UE 5.9), else the verb its name
// starts with (5.8's schemas carry no hint). A Blocked tool would run code outside this plugin's console policy and is
// never called, whatever it states.
enum class EToolClass : uint8 { Read, Write, Destructive, Blocked };

/** bRegistryAnnotates: some tool of the registry states a mode, so one that states none is execute. */
EToolClass ClassifyTool(const FString& FullToolName, const TSharedPtr<FJsonObject>& Schema = nullptr, bool bRegistryAnnotates = false);
/** Whether any registered tool states an access mode. */
bool AnyAnnotated(const TArray<TSharedPtr<FJsonValue>>& Toolsets);
const TCHAR* ToolClassName(EToolClass Class);

// Every registered toolset's schema: [{name, version, description, tools: [{name, description, inputSchema}]}].
// False, with OutError, when the registry is not loaded.
bool ReadToolsets(TArray<TSharedPtr<FJsonValue>>& OutToolsets, FString& OutError);

// EDITOR_TOOLSETS_UNAVAILABLE with Error, and the enable_plugin call for the plugin that is off (the registry, else
// All Toolsets) when this engine has it.
void SendUnavailable(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                     const FString& Error);

// The schema entry of one tool ("Toolset.Tool", any case), null when no registered toolset has it.
TSharedPtr<FJsonObject> FindTool(const TArray<TSharedPtr<FJsonValue>>& Toolsets, const FString& FullToolName);

bool HandleListToolsets(UEngineWireSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

// call_editor_tool (bDestructiveAllowed false) refuses a destructive tool and points at
// call_editor_tool_destructive, whose record demands the destructive scope and elevated consent.
bool HandleCallTool(UEngineWireSubsystem* Self, const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    bool bDestructiveAllowed);
}
