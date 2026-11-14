#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleGetMaterialNodeDetails(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_material_node_details")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();
    // A node is named by nodeId, or by its position in the expression list.
    FString NodeId;
    double ExpressionIndex = -1.0;
    const bool bHasIndex = Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);
    if ((!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) && !bHasIndex) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' (or 'expressionIndex')."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = nullptr;
    if (!NodeId.IsEmpty()) {
      Expr = Material ? FindExpressionByIdOrName(Material, NodeId) : FindExpressionByIdOrNameInFunction(Function, NodeId);
    } else {
      const auto& Exprs = Material ? MCP_GET_MATERIAL_EXPRESSIONS(Material) : MCP_GET_FUNCTION_EXPRESSIONS(Function);
      const int32 Index = static_cast<int32>(ExpressionIndex);
      if (Exprs.IsValidIndex(Index)) { Expr = Exprs[Index]; }
    }
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
    Result->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));

    // Parameter defaults and function pins, which get_material_info does not list (get_node_properties answers them
    // under its own keys).
    if (UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
      Result->SetNumberField(TEXT("scalarDefault"), McpHandlerUtils::ReadableFloat(ScalarParam->DefaultValue));
    } else if (UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
      Result->SetObjectField(TEXT("vectorDefault"), McpHandlerUtils::LinearColorToJson(VectorParam->DefaultValue));
    }

    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      Result->SetStringField(TEXT("inputName"), In->InputName.ToString());
      Result->SetStringField(TEXT("inputType"), FunctionInputTypeToString(In->InputType));
      Result->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
      Result->SetNumberField(TEXT("sortPriority"), In->SortPriority);
    } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      Result->SetStringField(TEXT("outputName"), Out->OutputName.ToString());
      Result->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Node details retrieved."), Result);
    return true;
  }

  return false;
}
}
