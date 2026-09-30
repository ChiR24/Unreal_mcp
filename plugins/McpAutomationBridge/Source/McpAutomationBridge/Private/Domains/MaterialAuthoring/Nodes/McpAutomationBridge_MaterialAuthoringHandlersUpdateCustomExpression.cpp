#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleUpdateCustomExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("update_custom_expression")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NodeId);
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UMaterialExpressionCustom *CustomExpr = Cast<UMaterialExpressionCustom>(Expr);
    if (!CustomExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node is not a Custom Expression."), TEXT("INVALID_NODE_TYPE"));
      return true;
    }

    FString NewCode;
    if (Payload->TryGetStringField(TEXT("code"), NewCode)) {
      CustomExpr->Code = NewCode;
    }

    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) {
      CustomExpr->Description = NewDesc;
    }

    FString NewOutputType;
    if (Payload->TryGetStringField(TEXT("outputType"), NewOutputType)) {
      CustomExpr->OutputType = ParseCustomOutputType(NewOutputType, CustomExpr->OutputType);
    }

    ApplyCustomInputs(CustomExpr, Payload);

    ApplyCustomAdditionalOutputs(CustomExpr, Payload);

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(HostOuter);
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("code"), CustomExpr->Code);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Custom expression updated."), Result);
    return true;
  }

  return false;
}
}
