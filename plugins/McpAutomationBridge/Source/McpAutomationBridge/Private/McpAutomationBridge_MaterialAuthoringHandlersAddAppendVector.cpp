#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddAppendVector(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_append")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionAppendVector *AppendExpr =
        NewObject<UMaterialExpressionAppendVector>(
            HostOuter, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
            RF_Transactional);
    AppendExpr->MaterialExpressionEditorX = (int32)X;
    AppendExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, AppendExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(AppendExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Append node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_custom_expression (Material or MaterialFunction host)
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
