#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
void SendBlueprintAddNodeResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString &RegistryKey, UEdGraph *TargetGraph, UEdGraphNode *NewNode,
    float PosX, float PosY, bool bSaved,
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

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintAction: blueprint_add_node completed Path=%s "
              "nodeGuid=%s"),
         *RegistryKey, *NewNode->NodeGuid.ToString());
}
}
