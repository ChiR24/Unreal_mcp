#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleRemoveMaterialNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("remove_material_node")) {
    FString AssetPath, NodeId;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialExpression *Expr = Material
        ? FindExpressionByIdOrName(Material, NodeId)
        : FindExpressionByIdOrNameInFunction(Function, NodeId);
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    // Remove the expression from the appropriate container
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

    if (Material) { Material->PostEditChange(); Material->MarkPackageDirty(); }
    else { Function->PostEditChange(); Function->MarkPackageDirty(); }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetBoolField(TEXT("removed"), true);

    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Material node removed."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_material_parameter
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
