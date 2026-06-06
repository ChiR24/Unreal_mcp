#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintAddNode(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_add_node")) ||
      ActionMatchesPattern(TEXT("add_node")) ||
      AlphaNumLower.Contains(TEXT("blueprintaddnode"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_add_node handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_add_node requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString NodeType;
    LocalPayload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("nodeType required"), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString GraphName;
    LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
    if (GraphName.IsEmpty())
      GraphName = TEXT("EventGraph");

    FString FunctionName;
    LocalPayload->TryGetStringField(TEXT("functionName"), FunctionName);
    FString VariableName;
    LocalPayload->TryGetStringField(TEXT("variableName"), VariableName);
    FString NodeName;
    LocalPayload->TryGetStringField(TEXT("nodeName"), NodeName);
    float PosX = 0.0f, PosY = 0.0f;
    LocalPayload->TryGetNumberField(TEXT("posX"), PosX);
    LocalPayload->TryGetNumberField(TEXT("posY"), PosY);

    // Declare RegistryKey outside the conditional blocks
    const FString RegistryKey = Path;

#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2

    if (GBlueprintBusySet.Contains(Path)) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint is busy"), nullptr,
                             TEXT("BLUEPRINT_BUSY"));
      return true;
    }

    GBlueprintBusySet.Add(Path);
    ON_SCOPE_EXIT {
      if (GBlueprintBusySet.Contains(Path)) {
        GBlueprintBusySet.Remove(Path);
      }
    };

    FString Normalized;
    FString LoadErr;
    UBlueprint *BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
    if (!BP) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"), LoadErr);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr,
                             Result, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: blueprint_add_node begin Path=%s "
                "nodeType=%s"),
           *RegistryKey, *NodeType);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("blueprint_add_node macro check: MCP_HAS_K2NODE_HEADERS=%d "
                "MCP_HAS_EDGRAPH_SCHEMA_K2=%d"),
           static_cast<int32>(MCP_HAS_K2NODE_HEADERS),
           static_cast<int32>(MCP_HAS_EDGRAPH_SCHEMA_K2));

    UEdGraph *TargetGraph = FindOrCreateBlueprintNodeGraph(BP, GraphName);

    if (!TargetGraph) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"),
                             TEXT("Failed to locate or create target graph"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Graph creation failed"), Result,
                             TEXT("GRAPH_ERROR"));
      return true;
    }

    BP->Modify();
    TargetGraph->Modify();

    FString NodeErrorMessage;
    FString NodeErrorCode;
    TSharedPtr<FJsonObject> NodeErrorResult;
    UEdGraphNode *NewNode =
        CreateBlueprintGraphNode(TargetGraph, BP, NodeType, FunctionName,
                                 VariableName, NodeName, NodeErrorMessage,
                                 NodeErrorCode, NodeErrorResult);

    if (!NewNode) {
      if (!NodeErrorCode.IsEmpty()) {
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                      NodeErrorMessage, NodeErrorResult,
                                      NodeErrorCode);
        return true;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("error"), TEXT("Failed to instantiate node"));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Node creation failed"), Result,
                             TEXT("NODE_CREATION_FAILED"));
      return true;
    }

    TargetGraph->Modify();
    TargetGraph->AddNode(NewNode, true, false);
    NewNode->SetFlags(RF_Transactional);
    NewNode->CreateNewGuid();
    NewNode->NodePosX = PosX;
    NewNode->NodePosY = PosY;
    NewNode->AllocateDefaultPins();
    NewNode->Modify();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

    bool bExecLinked = false;
    bool bValueLinked = false;
    LinkBlueprintGraphNodePins(TargetGraph, NewNode, bExecLinked, bValueLinked);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

    McpSafeCompileBlueprint(BP);
    const bool bSaved = SaveLoadedAssetThrottled(BP);

    SendBlueprintAddNodeResult(Bridge, RequestId, RequestingSocket, RegistryKey,
                               TargetGraph, NewNode, PosX, PosY, bSaved,
                               bExecLinked, bValueLinked, NodeName,
                               FunctionName, VariableName);
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_add_node requires editor build with K2 node headers"),
        nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // blueprint_connect_pins: Connect two pins between nodes
  return false;
}
#endif
} // namespace McpBlueprintHandlers
