#pragma once

#include "EdGraph/EdGraphNode.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

class FJsonValue;
class FMcpBridgeWebSocket;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UEdGraphSchema_K2;
class UFunction;
class UK2Node;
class UK2Node_VariableGet;
class UK2Node_VariableSet;
class USimpleConstructionScript;

struct FBPVariableDescription;
struct FEdGraphPinType;
struct FMemberReference;
struct FPinConnectionResponse;
class FProperty;
struct FUserPinInfo;


namespace McpBlueprintHandlers {
struct FBlueprintActionContext {
  UMcpAutomationBridgeSubsystem &Bridge;
  FString RequestId;
  FString Action;
  FString CleanAction;
  FString Lower;
  FString AlphaNumLower;
  TSharedPtr<FJsonObject> LocalPayload;
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
};

#define MCP_BLUEPRINT_ACTION_LOCALS(ContextName)                              \
  UMcpAutomationBridgeSubsystem &Bridge = (ContextName).Bridge;               \
  const FString &RequestId = (ContextName).RequestId;                         \
  const FString &Action = (ContextName).Action;                               \
  const FString &CleanAction = (ContextName).CleanAction;                     \
  const FString &Lower = (ContextName).Lower;                                 \
  const FString &AlphaNumLower = (ContextName).AlphaNumLower;                 \
  const TSharedPtr<FJsonObject> &Payload = (ContextName).LocalPayload;        \
  const TSharedPtr<FJsonObject> &LocalPayload = (ContextName).LocalPayload;   \
  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket =                          \
      (ContextName).RequestingSocket;                                         \
  auto ActionMatchesPattern = [&](const TCHAR *Pattern) -> bool {             \
    return McpBlueprintHandlers::ActionMatchesPattern(ContextName, Pattern);  \
  };                                                                          \
  auto ResolveBlueprintRequestedPath = [&]() -> FString {                     \
    return McpBlueprintHandlers::ResolveBlueprintRequestedPath(LocalPayload);  \
  }

#define MCP_BLUEPRINT_SCS_LOCALS(ContextName)                                 \
  MCP_BLUEPRINT_ACTION_LOCALS(ContextName);                                   \
  auto ResolveBlueprint = [&]() -> UBlueprint * {                             \
    return McpBlueprintHandlers::ResolveScsBlueprint(Payload);                \
  }

#include "Domains/Blueprint/McpAutomationBridge_BlueprintHandlersDeclarations.h"
} // namespace McpBlueprintHandlers
