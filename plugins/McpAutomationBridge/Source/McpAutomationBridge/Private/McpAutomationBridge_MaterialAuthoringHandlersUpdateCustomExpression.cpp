#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
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

    // Update code if provided
    FString NewCode;
    if (Payload->TryGetStringField(TEXT("code"), NewCode)) {
      CustomExpr->Code = NewCode;
    }

    // Update description if provided
    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) {
      CustomExpr->Description = NewDesc;
    }

    // Update output type if provided
    FString NewOutputType;
    if (Payload->TryGetStringField(TEXT("outputType"), NewOutputType)) {
      if (NewOutputType == TEXT("Float1")) CustomExpr->OutputType = CMOT_Float1;
      else if (NewOutputType == TEXT("Float2")) CustomExpr->OutputType = CMOT_Float2;
      else if (NewOutputType == TEXT("Float3")) CustomExpr->OutputType = CMOT_Float3;
      else if (NewOutputType == TEXT("Float4")) CustomExpr->OutputType = CMOT_Float4;
      else if (NewOutputType == TEXT("MaterialAttributes")) CustomExpr->OutputType = CMOT_MaterialAttributes;
    }

    // Update inputs if provided
    const TArray<TSharedPtr<FJsonValue>> *InputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray) {
      CustomExpr->Inputs.Empty();
      for (const auto &InputVal : *InputsArray) {
        const TSharedPtr<FJsonObject> *InputObj = nullptr;
        if (InputVal->TryGetObject(InputObj) && InputObj) {
          FString InputName;
          (*InputObj)->TryGetStringField(TEXT("name"), InputName);
          if (!InputName.IsEmpty()) {
            FCustomInput NewInput;
            NewInput.InputName = FName(*InputName);
            CustomExpr->Inputs.Add(NewInput);
          }
        }
      }
    }

    // Update additional outputs if provided
    const TArray<TSharedPtr<FJsonValue>> *OutputsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("additionalOutputs"), OutputsArray) && OutputsArray) {
      CustomExpr->AdditionalOutputs.Empty();
      for (const auto &OutputVal : *OutputsArray) {
        const TSharedPtr<FJsonObject> *OutputObj = nullptr;
        if (OutputVal->TryGetObject(OutputObj) && OutputObj) {
          FString OutputName, OType;
          (*OutputObj)->TryGetStringField(TEXT("name"), OutputName);
          (*OutputObj)->TryGetStringField(TEXT("type"), OType);
          if (!OutputName.IsEmpty()) {
            FCustomOutput NewOutput;
            NewOutput.OutputName = FName(*OutputName);
            if (OType == TEXT("Float2")) NewOutput.OutputType = CMOT_Float2;
            else if (OType == TEXT("Float3")) NewOutput.OutputType = CMOT_Float3;
            else if (OType == TEXT("Float4")) NewOutput.OutputType = CMOT_Float4;
            else if (OType == TEXT("MaterialAttributes")) NewOutput.OutputType = CMOT_MaterialAttributes;
            else NewOutput.OutputType = CMOT_Float1;
            CustomExpr->AdditionalOutputs.Add(NewOutput);
          }
        }
      }
    }

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("code"), CustomExpr->Code);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Custom expression updated."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_node_chain — trace signal path from startNodeId to endNodeId/endPin
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
