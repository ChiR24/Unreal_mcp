#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
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
  if (Bridge.ConnectionManager.IsValid()) {
    Bridge.ConnectionManager->SendControlMessage(Notify);
  }
}
#endif
}
