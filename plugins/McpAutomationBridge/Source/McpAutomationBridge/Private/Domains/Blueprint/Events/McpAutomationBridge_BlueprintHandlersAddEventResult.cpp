#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintHandlers {
namespace {

// blueprint.add_event declares `nodeGuid` as a REQUIRED output field. The
// gateway projects a handler result down to schema-declared names, so a
// response without it projected to an empty payload and every successful
// add_event — including the idempotent "event already exists" path, which is
// the common case for BeginPlay — was reported to the caller as
// OUTPUT_SCHEMA_VIOLATION while the node sat in the graph.
//
// The event node is located by title/name rather than threaded through the
// three call sites, so the idempotent path resolves the PRE-EXISTING node's
// GUID, which is the identifier a caller needs in order to wire it up.
FString FindEventNodeGuid(UBlueprint *BP, const FName &EventName) {
  if (!BP) {
    return FString();
  }
  const FString Wanted = EventName.ToString();
  TArray<UEdGraph *> Graphs;
  BP->GetAllGraphs(Graphs);
  // The event itself first: the title match below also hits a CallFunction node
  // titled after the event (a call to OnDeath), which build_graph would then alias.
  TArray<UK2Node_Event *> Events;
  FBlueprintEditorUtils::GetAllNodesOfClass(BP, Events);
  for (const UK2Node_Event *Event : Events) {
    if (!EventName.IsNone() && (Event->CustomFunctionName == EventName ||
                                (Event->bOverrideFunction && Event->EventReference.GetMemberName() == EventName))) {
      return Event->NodeGuid.ToString();
    }
  }
  for (const UEdGraph *Graph : Graphs) {
    if (!Graph) {
      continue;
    }
    for (const UEdGraphNode *Node : Graph->Nodes) {
      if (!Node) {
        continue;
      }
      // A component-bound event is titled after its delegate, so its
      // generated function name (BndEvt__...) matches only here.
      const UK2Node_Event *EventNode = Cast<UK2Node_Event>(Node);
      if (EventNode && !EventName.IsNone() && EventNode->CustomFunctionName == EventName) {
        return Node->NodeGuid.ToString();
      }
      const FString Title =
          Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
      // "Event BeginPlay"/"ReceiveBeginPlay"/"OnCollected" all resolve here:
      // the engine title carries the display form while GetName() carries the
      // K2Node object name, so both are compared.
      if (Title.Equals(Wanted, ESearchCase::IgnoreCase) ||
          Title.EndsWith(Wanted, ESearchCase::IgnoreCase) ||
          Node->GetName().Contains(Wanted)) {
        return Node->NodeGuid.ToString();
      }
    }
  }
  return FString();
}

} // namespace
void SendBlueprintAddEventResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *BP,
    const FString &RegistryKey, const FName &EventName,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params,
    bool bSaved) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
  Resp->SetStringField(TEXT("eventName"), EventName.ToString());
  Resp->SetStringField(TEXT("eventType"), FinalType);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  const FString EventNodeGuid = FindEventNodeGuid(BP, EventName);
  if (!EventNodeGuid.IsEmpty()) {
    Resp->SetStringField(TEXT("nodeGuid"), EventNodeGuid);
    Resp->SetStringField(TEXT("nodeId"), EventNodeGuid);
  }
  if (Params.Num() > 0) {
    Resp->SetArrayField(TEXT("parameters"), Params);
  }
  McpHandlerUtils::AddVerification(Resp, BP);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                TEXT("Event added"), Resp, FString());

}
}
