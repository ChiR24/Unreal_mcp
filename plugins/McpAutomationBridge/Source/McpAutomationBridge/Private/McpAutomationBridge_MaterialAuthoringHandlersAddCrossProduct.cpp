#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddCrossProduct(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_cross_product")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionCrossProduct *CrossExpr =
        NewObject<UMaterialExpressionCrossProduct>(
            HostOuter, UMaterialExpressionCrossProduct::StaticClass(), NAME_None,
            RF_Transactional);
    CrossExpr->MaterialExpressionEditorX = (int32)X;
    CrossExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, CrossExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(CrossExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("CrossProduct node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_desaturation
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
