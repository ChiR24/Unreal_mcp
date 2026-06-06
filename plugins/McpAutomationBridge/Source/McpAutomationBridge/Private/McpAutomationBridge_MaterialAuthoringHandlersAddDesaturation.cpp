#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddDesaturation(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_desaturation")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpressionDesaturation *DesatExpr =
        NewObject<UMaterialExpressionDesaturation>(
            HostOuter, UMaterialExpressionDesaturation::StaticClass(), NAME_None,
            RF_Transactional);

    // Set optional luminance factors
    const TSharedPtr<FJsonObject> *LumObj;
    if (Payload->TryGetObjectField(TEXT("luminanceFactors"), LumObj)) {
      double R = 0.3, G = 0.59, B = 0.11;
      (*LumObj)->TryGetNumberField(TEXT("r"), R);
      (*LumObj)->TryGetNumberField(TEXT("g"), G);
      (*LumObj)->TryGetNumberField(TEXT("b"), B);
      DesatExpr->LuminanceFactors = FLinearColor(R, G, B, 1.0f);
    }

    DesatExpr->MaterialExpressionEditorX = (int32)X;
    DesatExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, DesatExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(DesatExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Desaturation node added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_append (dedicated handler for convenience)
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
