#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintRemoveEvent(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("remove_event"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_remove_event requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }
    // Accept eventName, falling back to customEventName: the schema advertises
    // both and the TS layer forwards customEventName, but the handler used to
    // read only eventName -> a customEventName-only call was wrongly rejected
    // with "eventName required" (dogfood #30).
    FString EventName;
    LocalPayload->TryGetStringField(TEXT("eventName"), EventName);
    if (EventName.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("customEventName"), EventName);
    }
    // The published capability schema identifies the event by `nodeId` (the
    // node GUID returned by add_event/get_graph_details) and declares neither
    // eventName nor customEventName, so a schema-correct call must be accepted
    // by node id too.
    FString NodeId;
    LocalPayload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      LocalPayload->TryGetStringField(TEXT("nodeGuid"), NodeId);
    }
    if (EventName.IsEmpty() && NodeId.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("nodeId (or eventName / customEventName) required"),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString NormPath;
    const FString RegistryPath =
        (FindBlueprintNormalizedPath(Path, NormPath) && !NormPath.IsEmpty())
            ? NormPath
            : Path;

    // The EventGraph is the source of truth (dogfood #30). The per-asset
    // registry can be out of sync with the real graph -- e.g. a custom event
    // created via add_node never enters the registry -- so deciding
    // found/not-found from the registry returned a bogus "idempotent success"
    // while the node stayed in the graph (silent no-op). Search the actual
    // graph and remove there; the registry is only kept in sync afterwards.
    bool bBlueprintExists = false;
    int32 RemovedNodeCount = 0;
    FString NormalizedRemove;
    FString RemoveLoadErr;
    UBlueprint *RemoveBlueprint =
        LoadBlueprintAsset(RegistryPath, NormalizedRemove, RemoveLoadErr);
    bBlueprintExists = (RemoveBlueprint != nullptr);
    // Key registry ops off the actually-resolved asset path so add_event and
    // remove_event agree on the same key (add_event keys off LoadBlueprintAsset's
    // normalized path); a normalization mismatch would otherwise strand the
    // registry entry.
    const FString RegistryKey =
        (RemoveBlueprint && !NormalizedRemove.IsEmpty()) ? NormalizedRemove
                                                         : RegistryPath;
    // graphName picks the event graph page to search; it used to be ignored.
    FString GraphName;
    LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
    FString GraphError;
    UEdGraph *RemoveGraph = RemoveBlueprint
        ? FindBlueprintEventGraph(RemoveBlueprint, GraphName, GraphError)
        : nullptr;
    if (!GraphError.IsEmpty()) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, GraphError,
                                    nullptr, TEXT("GRAPH_NOT_FOUND"));
      return true;
    }
    if (RemoveBlueprint) {
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
    }
    if (!bBlueprintExists) {
      // Fall back to the asset registry to distinguish "blueprint missing"
      // from "blueprint present but event absent".
      bBlueprintExists = FindBlueprintNormalizedPath(RegistryPath, NormPath);
    }
    if (!bBlueprintExists) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Blueprint not found."), Resp,
                             TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Graph is authoritative: removed nothing => the event does not exist. Report NOT_FOUND loudly
    // instead of the old bogus idempotent success that masked the no-op
    // (dogfood #30).
    if (RemovedNodeCount == 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
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
    Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
    Resp->SetNumberField(TEXT("removedNodeCount"), RemovedNodeCount);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Event removed."),
        Resp, FString());
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("HandleBlueprintAction: event '%s' removed from '%s' (%d node(s))"),
           *EventName, *RegistryKey, RemovedNodeCount);
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
