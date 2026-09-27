#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopeExit.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddNode(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_node"))) {
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
    // The published add_node schema declares memberName, not functionName/
    // variableName, so the only spelling a caller can legally send was the one
    // never read: {nodeType:"GetVariable", memberName:"X"} produced a
    // K2Node_VariableGet with an unset VariableReference and therefore zero
    // pins, reported as success. Backfill from the declared field.
    FString MemberName;
    LocalPayload->TryGetStringField(TEXT("memberName"), MemberName);
    if (VariableName.IsEmpty()) VariableName = MemberName;
    if (FunctionName.IsEmpty()) FunctionName = MemberName;
    if (NodeName.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("customEventName"), NodeName);
    }
    if (NodeName.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("eventName"), NodeName);
    }
    if (NodeName.IsEmpty()) NodeName = MemberName;
    FString TargetClass;
    LocalPayload->TryGetStringField(TEXT("targetClass"), TargetClass);
    // Backfill from legacy/alternate payload fields so cast and CreateWidget
    // nodes created by existing callers (which send memberClass/nodeClass/
    // widgetType, or encode the class in a "CastTo<Class>" nodeType) still
    // resolve a target. Mirrors ReadTargetClassPayload on the create_node path.
    if (TargetClass.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("memberClass"), TargetClass);
    }
    if (TargetClass.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("nodeClass"), TargetClass);
    }
    if (TargetClass.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("widgetType"), TargetClass);
    }
    if (TargetClass.IsEmpty() &&
        NodeType.StartsWith(TEXT("CastTo"), ESearchCase::IgnoreCase)) {
      TargetClass = NodeType.Mid(6);
    }
    float PosX = 0.0f, PosY = 0.0f;
    LocalPayload->TryGetNumberField(TEXT("posX"), PosX);
    LocalPayload->TryGetNumberField(TEXT("posY"), PosY);

    // Declare RegistryKey outside the conditional blocks
    const FString RegistryKey = Path;



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
                                 VariableName, NodeName, TargetClass,
                                 NodeErrorMessage, NodeErrorCode,
                                 NodeErrorResult);

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
    // Allocate pins BEFORE PostPlacedNewNode(): checked pin accessors inside
    // PostPlacedNewNode() assert when the pin list is still empty (EdGraphNode.h:586).
    if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
    NewNode->PostPlacedNewNode();
    // Refuse stacked placements before any links are made: the node is already
    // registered, so remove it and fail with coordinates + free slots.
    {
      FString OverlapMessage;
      TSharedPtr<FJsonObject> OverlapDetails;
      if (McpGraphLayout::RefuseOverlappingNode(TargetGraph, NewNode, PosX, PosY, OverlapMessage, OverlapDetails))
      {
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                               OverlapMessage, OverlapDetails,
                               TEXT("NODE_OVERLAP"));
        return true;
      }
    }
    NewNode->Modify();

    // Left unwired, as the editor does on a drop: auto-wiring a Set from its own
    // Get, or hanging it off whatever event came first, built graphs that
    // compiled clean and did the wrong thing. connect_pins decides the wiring.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);

    McpSafeCompileBlueprint(BP);
    const bool bSaved = SaveLoadedAssetThrottled(BP);

    SendBlueprintAddNodeResult(Bridge, RequestId, RequestingSocket, RegistryKey,
                               TargetGraph, NewNode, PosX, PosY, bSaved, NodeName,
                               FunctionName, VariableName);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
