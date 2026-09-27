#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"

namespace McpBlueprintHandlers {
void SendBlueprintAddFunctionResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *Blueprint,
    const FString &RegistryKey, const FString &FuncName, bool bIsPublic,
    const TArray<TSharedPtr<FJsonValue>> &Inputs,
    const TArray<TSharedPtr<FJsonValue>> &Outputs, bool bSaved) {
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

}
}
