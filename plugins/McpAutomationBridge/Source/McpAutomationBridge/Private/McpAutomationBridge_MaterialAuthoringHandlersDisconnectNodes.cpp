#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleDisconnectNodes(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("disconnect_nodes")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Disconnect from main / output node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      if (Material) {
        if (!PinName.IsEmpty()) {
          bool bFound = false;
#if WITH_EDITORONLY_DATA
          if (PinName == TEXT("BaseColor")) {
            MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("EmissiveColor")) {
            MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("Roughness")) {
            MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("Metallic")) {
            MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("Specular")) {
            MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("Normal")) {
            MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("Opacity")) {
            MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("OpacityMask")) {
            MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("AmbientOcclusion")) {
            MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("SubsurfaceColor")) {
            MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = nullptr;
            bFound = true;
          } else if (PinName == TEXT("WorldPositionOffset")) {
            MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset).Expression = nullptr;
            bFound = true;
          }
#endif

          if (bFound) {
            FINALIZE_HOST();
            Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   TEXT("Disconnected from main material pin."));
            return true;
          }
        }
        Bridge->SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnect operation completed."));
        return true;
      } else {
        // UMaterialFunction host — clear FunctionOutput's A.Expression by name (or first/all if empty)
        bool bCleared = false;
#if WITH_EDITORONLY_DATA
        for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
          if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
            if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
              Out->A.Expression = nullptr;
              bCleared = true;
              if (!PinName.IsEmpty()) break;
            }
          }
        }
#endif
        if (bCleared) {
          FINALIZE_HOST();
          Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnected from function output."));
        } else {
          Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnect operation completed (no matching output)."));
        }
        return true;
      }
    }

    // Disconnect a specific input pin on a named expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!TargetExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    if (!PinName.IsEmpty()) {
      FProperty *Prop = TargetExpr->GetClass()->FindPropertyByName(FName(*PinName));
      if (Prop) {
        if (FStructProperty *StructProp = CastField<FStructProperty>(Prop)) {
          FExpressionInput *InputPtr =
              StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
          if (InputPtr) {
            InputPtr->Expression = nullptr;
            FINALIZE_HOST();
            Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   TEXT("Input pin disconnected."));
            return true;
          }
        }
      }
      Bridge->SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Input pin '%s' not found."), *PinName),
          TEXT("PIN_NOT_FOUND"));
      return true;
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Disconnect operation completed."));
    return true;
  }

  // ==========================================================================
  // 8.3 Material Functions
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_material_function
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
