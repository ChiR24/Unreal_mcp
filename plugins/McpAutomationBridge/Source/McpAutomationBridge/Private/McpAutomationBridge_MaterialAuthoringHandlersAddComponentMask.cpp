#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddComponentMask(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_component_mask")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    bool bR = true, bG = true, bB = true, bA = false;
    Payload->TryGetBoolField(TEXT("r"), bR);
    Payload->TryGetBoolField(TEXT("g"), bG);
    Payload->TryGetBoolField(TEXT("b"), bB);
    Payload->TryGetBoolField(TEXT("a"), bA);

    UMaterialExpressionComponentMask *MaskExpr =
        NewObject<UMaterialExpressionComponentMask>(
            HostOuter, UMaterialExpressionComponentMask::StaticClass(), NAME_None,
            RF_Transactional);
    MaskExpr->R = bR ? 1 : 0;
    MaskExpr->G = bG ? 1 : 0;
    MaskExpr->B = bB ? 1 : 0;
    MaskExpr->A = bA ? 1 : 0;
    MaskExpr->MaterialExpressionEditorX = (int32)X;
    MaskExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, MaskExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(MaskExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("ComponentMask node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_dot_product
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
