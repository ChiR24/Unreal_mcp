#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
void SendBlueprintAddNodeResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString &RegistryKey, UEdGraph *TargetGraph, UEdGraphNode *NewNode,
    float PosX, float PosY, bool bSaved, bool bExecLinked, bool bValueLinked,
    const FString &NodeName, const FString &FunctionName,
    const FString &VariableName) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("blueprintPath"), RegistryKey);
  Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
  Result->SetStringField(TEXT("nodeClass"), NewNode->GetClass()->GetName());
  Result->SetNumberField(TEXT("posX"), PosX);
  Result->SetNumberField(TEXT("posY"), PosY);
  Result->SetBoolField(TEXT("saved"), bSaved);
  Result->SetStringField(TEXT("nodeGuid"), NewNode->NodeGuid.ToString());
  if (Cast<UK2Node_VariableSet>(NewNode)) {
    Result->SetBoolField(TEXT("valueLinked"), bValueLinked);
    Result->SetBoolField(TEXT("execLinked"), bExecLinked);
  }
  if (!NodeName.IsEmpty()) {
    Result->SetStringField(TEXT("nodeName"), NodeName);
  }
  if (!FunctionName.IsEmpty()) {
    Result->SetStringField(TEXT("functionName"), FunctionName);
  }
  if (!VariableName.IsEmpty()) {
    Result->SetStringField(TEXT("variableName"), VariableName);
  }

  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                TEXT("Node added"), Result, FString());

  TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
  Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
  Notify->SetStringField(TEXT("event"), TEXT("add_node_completed"));
  Notify->SetStringField(TEXT("requestId"), RequestId);
  Notify->SetObjectField(TEXT("result"), Result);
  if (Bridge.ConnectionManager.IsValid()) {
    Bridge.ConnectionManager->SendControlMessage(Notify);
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintAction: blueprint_add_node completed Path=%s "
              "nodeGuid=%s"),
         *RegistryKey, *NewNode->NodeGuid.ToString());
}
#endif
}
