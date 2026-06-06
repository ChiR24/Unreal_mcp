#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddDotProduct(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_dot_product")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionDotProduct *DotExpr =
        NewObject<UMaterialExpressionDotProduct>(
            HostOuter, UMaterialExpressionDotProduct::StaticClass(), NAME_None,
            RF_Transactional);
    DotExpr->MaterialExpressionEditorX = (int32)X;
    DotExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, DotExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(DotExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("DotProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_cross_product
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
