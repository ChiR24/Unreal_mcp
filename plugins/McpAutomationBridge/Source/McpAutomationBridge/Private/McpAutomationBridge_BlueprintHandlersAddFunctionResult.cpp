#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
void SendBlueprintAddFunctionResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *Blueprint,
    const FString &RegistryKey, const FString &FuncName, bool bIsPublic,
    const TArray<TSharedPtr<FJsonValue>> &Inputs,
    const TArray<TSharedPtr<FJsonValue>> &Outputs, bool bSaved) {
  TSharedPtr<FJsonObject> Entry =
      FMcpAutomationBridge_EnsureBlueprintEntry(RegistryKey);
  TArray<TSharedPtr<FJsonValue>> Funcs =
      Entry->HasField(TEXT("functions"))
          ? Entry->GetArrayField(TEXT("functions"))
          : TArray<TSharedPtr<FJsonValue>>();
  bool bFound = false;
  for (const TSharedPtr<FJsonValue> &Value : Funcs) {
    if (!Value.IsValid() || Value->Type != EJson::Object) {
      continue;
    }
    const TSharedPtr<FJsonObject> Obj = Value->AsObject();
    if (!Obj.IsValid()) {
      continue;
    }

    FString Existing;
    if (Obj->TryGetStringField(TEXT("name"), Existing) &&
        Existing.Equals(FuncName, ESearchCase::IgnoreCase)) {
      Obj->SetBoolField(TEXT("public"), bIsPublic);
      if (Inputs.Num() > 0) {
        Obj->SetArrayField(TEXT("inputs"), Inputs);
      } else {
        Obj->RemoveField(TEXT("inputs"));
      }
      if (Outputs.Num() > 0) {
        Obj->SetArrayField(TEXT("outputs"), Outputs);
      } else {
        Obj->RemoveField(TEXT("outputs"));
      }
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    TSharedPtr<FJsonObject> Rec = McpHandlerUtils::CreateResultObject();
    Rec->SetStringField(TEXT("name"), FuncName);
    Rec->SetBoolField(TEXT("public"), bIsPublic);
    if (Inputs.Num() > 0) {
      Rec->SetArrayField(TEXT("inputs"), Inputs);
    }
    if (Outputs.Num() > 0) {
      Rec->SetArrayField(TEXT("outputs"), Outputs);
    }
    Funcs.Add(MakeShared<FJsonValueObject>(Rec));
  }

  Entry->SetArrayField(TEXT("functions"), Funcs);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("blueprintPath"), RegistryKey);
  Resp->SetStringField(TEXT("functionName"), FuncName);
  Resp->SetBoolField(TEXT("public"), bIsPublic);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  if (Inputs.Num() > 0) {
    Resp->SetArrayField(TEXT("inputs"), Inputs);
  }
  if (Outputs.Num() > 0) {
    Resp->SetArrayField(TEXT("outputs"), Outputs);
  }
  McpHandlerUtils::AddVerification(Resp, Blueprint);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                TEXT("Function added"), Resp, FString());

  TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
  Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
  Notify->SetStringField(TEXT("event"), TEXT("add_function_completed"));
  Notify->SetStringField(TEXT("requestId"), RequestId);
  Notify->SetObjectField(TEXT("result"), Resp);
  if (Bridge.ConnectionManager.IsValid()) {
    Bridge.ConnectionManager->SendControlMessage(Notify);
  }
}
#endif
}
