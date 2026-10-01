#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleDisconnectNodes(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("disconnect_nodes")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Every miss below used to answer "Disconnect operation completed." with nothing unplugged.
    // Disconnect from main / output node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      if (Material) {
        FExpressionInput* MainInput = PinName.IsEmpty() ? nullptr : GetMainMaterialInput(Material, NormalizeMaterialInputName(PinName));
        if (!MainInput) {
          Bridge->SendAutomationError(Socket, RequestId,
              FString::Printf(TEXT("'%s' is not a material output input. Main inputs: %s."), *PinName, *ListMainMaterialInputs(Material)),
              TEXT("PIN_NOT_FOUND"));
          return true;
        }
        MainInput->Expression = nullptr;
        FINALIZE_HOST();
        Bridge->SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnected from main material pin."), McpMaterialHostResult(HostOuter));
        return true;
      }
      // UMaterialFunction host — clear FunctionOutput's A.Expression by name (or all if empty)
      bool bCleared = false;
      for (UMaterialExpression *Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
            Out->A.Expression = nullptr;
            bCleared = true;
            if (!PinName.IsEmpty()) break;
          }
        }
      }
      if (!bCleared) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("No function output named '%s'."), *PinName), TEXT("PIN_NOT_FOUND"));
        return true;
      }
      FINALIZE_HOST();
      Bridge->SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Disconnected from function output."), McpMaterialHostResult(HostOuter));
      return true;
    }

    // Disconnect a specific input pin on a named expression
    UMaterialExpression *TargetExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!TargetExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // By property name first, then by label as connect_nodes matches them: a custom node's or a
    // function call's inputs ("OB", "UVs (Vector2)") are not properties, so they never unplugged.
    FExpressionInput *Target = nullptr;
    if (FStructProperty *StructProp = CastField<FStructProperty>(TargetExpr->GetClass()->FindPropertyByName(FName(*PinName)))) {
      Target = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
    }
    FString Available;
    for (int32 InputIndex = 0; !Target && TargetExpr->GetInput(InputIndex); ++InputIndex) {
      const FString Label = TargetExpr->GetInputName(InputIndex).ToString();
      FString Plain;
      if (!Label.Split(TEXT(" ("), &Plain, nullptr)) {
        Plain = Label;
      }
      if (!PinName.IsEmpty() && (Label.Equals(PinName, ESearchCase::IgnoreCase) || Plain.Equals(PinName, ESearchCase::IgnoreCase))) {
        Target = TargetExpr->GetInput(InputIndex);
      }
      Available += FString::Printf(TEXT("%s%s"), Available.IsEmpty() ? TEXT("") : TEXT(", "), *Label);
    }
    if (!Target) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Input pin '%s' not found on %s. Its inputs: %s."), *PinName,
                          *TargetExpr->GetClass()->GetName(), Available.IsEmpty() ? TEXT("<none>") : *Available),
          TEXT("PIN_NOT_FOUND"));
      return true;
    }
    Target->Expression = nullptr;
    FINALIZE_HOST();
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Input pin disconnected."), McpMaterialHostResult(HostOuter));
    return true;
  }

  return false;
}
}
