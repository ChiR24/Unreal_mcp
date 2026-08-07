#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
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
  for (const UEdGraph *Graph : Graphs) {
    if (!Graph) {
      continue;
    }
    for (const UEdGraphNode *Node : Graph->Nodes) {
      if (!Node) {
        continue;
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
  TSharedPtr<FJsonObject> Entry =
      FMcpAutomationBridge_EnsureBlueprintEntry(RegistryKey);
  TArray<TSharedPtr<FJsonValue>> Events =
      Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events"))
                                      : TArray<TSharedPtr<FJsonValue>>();
  bool bFound = false;
  for (const TSharedPtr<FJsonValue> &Item : Events) {
    if (!Item.IsValid() || Item->Type != EJson::Object) {
      continue;
    }
    const TSharedPtr<FJsonObject> Obj = Item->AsObject();
    if (Obj.IsValid()) {
      FString Existing;
      if (Obj->TryGetStringField(TEXT("name"), Existing) &&
          Existing.Equals(EventName.ToString(), ESearchCase::IgnoreCase)) {
        Obj->SetStringField(TEXT("eventType"), FinalType);
        if (Params.Num() > 0) {
          Obj->SetArrayField(TEXT("parameters"), Params);
        } else {
          Obj->RemoveField(TEXT("parameters"));
        }
        bFound = true;
        break;
      }
    }
  }

  if (!bFound) {
    TSharedPtr<FJsonObject> Rec = McpHandlerUtils::CreateResultObject();
    Rec->SetStringField(TEXT("name"), EventName.ToString());
    Rec->SetStringField(TEXT("eventType"), FinalType);
    if (Params.Num() > 0) {
      Rec->SetArrayField(TEXT("parameters"), Params);
    }
    Events.Add(MakeShared<FJsonValueObject>(Rec));
  }

  Entry->SetArrayField(TEXT("events"), Events);

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

  TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
  Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
  Notify->SetStringField(TEXT("event"), TEXT("add_event_completed"));
  Notify->SetStringField(TEXT("requestId"), RequestId);
  Notify->SetObjectField(TEXT("result"), Resp);
  Bridge.BroadcastAutomationEvent(Notify, RequestingSocket);
}
#endif
}
