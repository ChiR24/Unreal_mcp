#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddConditionalNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_if") || SubAction == TEXT("add_switch")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_if")) {
      NewExpr = NewObject<UMaterialExpressionIf>(
          HostOuter, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("If");
    } else {
      // Switch can be implemented via StaticSwitch or If
      NewExpr = NewObject<UMaterialExpressionIf>(
          HostOuter, UMaterialExpressionIf::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Switch");
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, NewExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(NewExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("%s node added."), *NodeName),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_component_mask
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
