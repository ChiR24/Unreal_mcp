#include "Domains/Blueprint/McpBPActionContext.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCallers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintRemoveEvent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("remove_event"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_remove_event"));
    if (Path.IsEmpty()) {
      return true;
    }
    // eventName names a custom event; nodeId (the node GUID add_event and get_graph_details hand out, or the node's
    // object name) names any event node, an inherited override included.
    FString EventName;
    LocalPayload->TryGetStringField(TEXT("eventName"), EventName);
    FString NodeId;
    LocalPayload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (EventName.IsEmpty() && NodeId.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("nodeId or eventName required"),
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Whether the event exists is read from the event graph itself, which is searched and changed directly.
    int32 RemovedNodeCount = 0;
    FString ResolvedPath;
    FString RemoveLoadErr;
    UBlueprint *RemoveBlueprint = LoadBlueprintAsset(Path, ResolvedPath, RemoveLoadErr);
    if (!RemoveBlueprint) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("blueprintPath"), Path);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found."), Resp,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }
    if (ResolvedPath.IsEmpty()) {
      ResolvedPath = Path;
    }
    // graphName picks the event graph page to search.
    FString GraphName;
    LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
    FString GraphError;
    UEdGraph *RemoveGraph = FindBlueprintEventGraph(RemoveBlueprint, GraphName, GraphError);
    if (!GraphError.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId, GraphError,
                                 TEXT("GRAPH_NOT_FOUND"));
      return true;
    }
    if (RemoveGraph) {
      TArray<UEdGraphNode *> NodesToRemove;
      for (UEdGraphNode *Node : RemoveGraph->Nodes) {
        if (!Node) {
          continue;
        }
        if (!NodeId.IsEmpty()) {
          // Match by node GUID (the id add_event / get_graph_details hand
          // out) or the node's object name; any event node qualifies so an
          // inherited event override can be removed by id as well.
          const bool bIdMatches =
              Node->NodeGuid.ToString().Equals(NodeId, ESearchCase::IgnoreCase) ||
              Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens)
                  .Equals(NodeId, ESearchCase::IgnoreCase) ||
              Node->GetName().Equals(NodeId, ESearchCase::IgnoreCase);
          if (bIdMatches && Node->IsA<UK2Node_Event>()) {
            NodesToRemove.Add(Node);
            if (EventName.IsEmpty()) {
              if (UK2Node_CustomEvent *CustomEvent = Cast<UK2Node_CustomEvent>(Node)) {
                EventName = CustomEvent->CustomFunctionName.ToString();
              } else if (UK2Node_Event *EventNode = Cast<UK2Node_Event>(Node)) {
                EventName = EventNode->EventReference.GetMemberName().ToString();
              }
            }
          }
          continue;
        }
        if (UK2Node_CustomEvent *CustomEvent =
                Cast<UK2Node_CustomEvent>(Node)) {
          if (CustomEvent->CustomFunctionName.ToString().Equals(
                  EventName, ESearchCase::IgnoreCase)) {
            NodesToRemove.Add(CustomEvent);
          }
        }
      }
      // A custom event still called would leave each call calling nothing, and the Blueprint stopped compiling.
      TArray<FString> Callers;
      for (UEdGraphNode *Node : NodesToRemove) {
        if (const UK2Node_CustomEvent *Custom = Cast<UK2Node_CustomEvent>(Node)) {
          Callers.Append(McpBlueprintCallersOf(RemoveBlueprint, Custom->CustomFunctionName));
        }
      }
      if (Callers.Num() > 0) {
        TSharedPtr<FJsonObject> Refusal = McpHandlerUtils::CreateResultObject();
        Refusal->SetArrayField(TEXT("callers"), McpHandlerUtils::ToJsonStringArray(Callers));
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("%s is still called %d time(s): %s. Delete those call nodes first (manage_blueprint delete_node), then remove it."),
                            *EventName, Callers.Num(), *FString::Join(Callers, TEXT(", "))),
            Refusal, TEXT("EVENT_IN_USE"));
        return true;
      }
      if (NodesToRemove.Num() > 0) {
        RemoveGraph->Modify();
        for (UEdGraphNode *Node : NodesToRemove) {
          RemoveGraph->RemoveNode(Node);
        }
        RemovedNodeCount = NodesToRemove.Num();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(
            RemoveBlueprint);
        McpSafeCompileBlueprint(RemoveBlueprint);
        SaveLoadedAssetThrottled(RemoveBlueprint);
      }
    }
    // Nothing removed: the graph holds no such event.
    if (RemovedNodeCount == 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("blueprintPath"), ResolvedPath);
      Resp->SetStringField(
          TEXT("hint"),
          TEXT("No custom event with this name in the event graph. For inherited "
               "event overrides (e.g. ReceiveBeginPlay) use delete_node."));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Custom event not found."), Resp,
                             TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("eventName"), EventName);
    Resp->SetStringField(TEXT("blueprintPath"), ResolvedPath);
    Resp->SetNumberField(TEXT("removedNodeCount"), RemovedNodeCount);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Event removed."),
        Resp, FString());
    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("HandleBlueprintAction: event '%s' removed from '%s' (%d node(s))"),
           *EventName, *ResolvedPath, RemovedNodeCount);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
