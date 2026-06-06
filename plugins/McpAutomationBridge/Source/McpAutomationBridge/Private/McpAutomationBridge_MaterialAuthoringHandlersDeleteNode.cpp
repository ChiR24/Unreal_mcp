#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleDeleteNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("delete_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    // Accept single nodeId or array of nodeIds
    TArray<FString> NodeIds;
    FString SingleId;
    if (Payload->TryGetStringField(TEXT("nodeId"), SingleId) && !SingleId.IsEmpty()) {
      NodeIds.Add(SingleId);
    }
    const TArray<TSharedPtr<FJsonValue>> *IdsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("nodeIds"), IdsArr) && IdsArr) {
      for (const auto &Val : *IdsArr) {
        FString Id;
        if (Val->TryGetString(Id) && !Id.IsEmpty()) NodeIds.Add(Id);
      }
    }
    if (NodeIds.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'nodeIds'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    TArray<FString> Removed;
    for (const FString &NId : NodeIds) {
      UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NId);
      if (!Expr) continue;

      // Auto-disconnect: clear all references to this node from other expressions
      for (UMaterialExpression *Other : AllExpr) {
        if (!Other || Other == Expr) continue;
        for (TFieldIterator<FStructProperty> It(Other->GetClass()); It; ++It) {
          FStructProperty *SP = *It;
          if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
          FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Other);
          if (InPtr && InPtr->Expression == Expr) {
            InPtr->Expression = nullptr;
            InPtr->OutputIndex = 0;
          }
        }
        if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Other)) {
          for (FCustomInput &CI : CE->Inputs) {
            if (CI.Input.Expression == Expr) { CI.Input.Expression = nullptr; CI.Input.OutputIndex = 0; }
          }
        }
        if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Other)) {
          for (FFunctionExpressionInput &FI : MFC->FunctionInputs) {
            if (FI.Input.Expression == Expr) { FI.Input.Expression = nullptr; FI.Input.OutputIndex = 0; }
          }
        }
      }

      // Clear Material main pin references
      if (Material) {
#if WITH_EDITORONLY_DATA
        auto ClearMainPin = [&](FExpressionInput &Input) {
          if (Input.Expression == Expr) { Input.Expression = nullptr; Input.OutputIndex = 0; }
        };
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Roughness));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Metallic));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Specular));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Normal));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Opacity));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
        ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
      }

      // Remove the expression
      if (Material) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Material->GetExpressionCollection().RemoveExpression(Expr);
#else
        Material->Expressions.Remove(Expr);
#endif
      } else {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        Function->GetExpressionCollection().RemoveExpression(Expr);
#else
        Function->FunctionExpressions.Remove(Expr);
#endif
      }
      Removed.Add(NId);
    }

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> RemovedArr;
    for (const FString &R : Removed) {
      RemovedArr.Add(MakeShared<FJsonValueString>(R));
    }
    Result->SetArrayField(TEXT("removed"), RemovedArr);
    Result->SetNumberField(TEXT("removedCount"), Removed.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Deleted %d node(s)."), Removed.Num()),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // update_custom_expression — modify code/inputs/outputs of existing CE
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
