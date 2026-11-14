#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Timeline/McpBPTimeline.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Foundation/GraphLayout/McpGraphOverlap.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintAddNode(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("add_node"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_add_node"));
    if (Path.IsEmpty()) {
      return true;
    }

    FString NodeType;
    LocalPayload->TryGetStringField(TEXT("nodeType"), NodeType);
    if (NodeType.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("nodeType required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString GraphName;
    LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
    if (GraphName.IsEmpty())
      GraphName = TEXT("EventGraph");

    FString FunctionName;
    LocalPayload->TryGetStringField(TEXT("functionName"), FunctionName);
    FString NodeName;
    LocalPayload->TryGetStringField(TEXT("nodeName"), NodeName);
    // memberName names the variable of a variable node, and stands in for functionName and nodeName when they are not
    // sent: {nodeType:"GetVariable", memberName:"X"} is the call the record describes.
    FString MemberName;
    LocalPayload->TryGetStringField(TEXT("memberName"), MemberName);
    FString VariableName = MemberName;
    if (FunctionName.IsEmpty()) FunctionName = MemberName;
    if (NodeName.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("eventName"), NodeName);
    }
    if (NodeName.IsEmpty()) NodeName = MemberName;
    FString TargetClass;
    LocalPayload->TryGetStringField(TEXT("targetClass"), TargetClass);
    // A cast nodeType may carry its class: "CastTo<Class>".
    if (TargetClass.IsEmpty() &&
        NodeType.StartsWith(TEXT("CastTo"), ESearchCase::IgnoreCase)) {
      TargetClass = NodeType.Mid(6);
    }
    float PosX = 0.0f, PosY = 0.0f;
    LocalPayload->TryGetNumberField(TEXT("posX"), PosX);
    LocalPayload->TryGetNumberField(TEXT("posY"), PosY);

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

    UE_LOG(LogEngineWireSubsystem, Log,
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
      if (McpGraphLayout::RefuseOverlappingNode(TargetGraph, NewNode, PosX, PosY, OverlapMessage, OverlapDetails,
                                                McpGraphLayout::NamesNoPosition(LocalPayload)))
      {
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                               OverlapMessage, OverlapDetails,
                               TEXT("NODE_OVERLAP"));
        return true;
      }
    }
    FString TimelineName, TimelineError;
    LocalPayload->TryGetStringField(TEXT("timelineName"), TimelineName);
    if (!McpBindTimelineNode(NewNode, BP, TimelineName, TimelineError)) {
      Bridge.SendAutomationError(RequestingSocket, RequestId, TimelineError, TEXT("INVALID_ARGUMENT"));
      return true;
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
