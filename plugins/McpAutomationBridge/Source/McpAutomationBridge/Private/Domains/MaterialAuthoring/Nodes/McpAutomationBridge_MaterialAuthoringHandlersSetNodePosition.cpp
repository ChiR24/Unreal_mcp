#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
/**
 * Move an existing material expression.
 *
 * Nodes could be created at a coordinate but never moved afterwards, so a graph
 * laid out badly stayed that way: the only recourse was remove_material_node
 * plus a re-add, which drops every connection the node had. Repositioning is
 * what makes the overlap warning on creation actionable after the fact.
 */
bool HandleSetNodePosition(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_node_position")) {
    return false;
  }

  FString AssetPath, NodeId;
  if ((!Payload->TryGetStringField(TEXT("materialPath"), AssetPath) || AssetPath.IsEmpty()) &&
      (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' (or 'assetPath')."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // The node adders read `x`/`y` first and fall back to `posX`/`posY`; keep the
  // same spelling contract here so a caller can reuse the coordinates it was
  // handed back at creation time.
  double PosX = 0.0;
  double PosY = 0.0;
  const bool bHasX = Payload->TryGetNumberField(TEXT("x"), PosX) ||
                     Payload->TryGetNumberField(TEXT("posX"), PosX);
  const bool bHasY = Payload->TryGetNumberField(TEXT("y"), PosY) ||
                     Payload->TryGetNumberField(TEXT("posY"), PosY);
  if (!bHasX || !bHasY) {
    Bridge->SendAutomationError(Socket, RequestId,
                        TEXT("Both 'x' and 'y' (or 'posX'/'posY') are required."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

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

  Expr->Modify();
  Expr->MaterialExpressionEditorX = static_cast<int32>(PosX);
  Expr->MaterialExpressionEditorY = static_cast<int32>(PosY);
  FINALIZE_HOST();

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
  const FString PlacementWarning =
      AddMaterialNodePlacementFields(Result, Material, Expr);
  Bridge->SendAutomationResponse(
      Socket, RequestId, true,
      PlacementWarning.IsEmpty()
          ? FString::Printf(TEXT("Node '%s' moved to (%d, %d)."), *NodeId,
                            Expr->MaterialExpressionEditorX,
                            Expr->MaterialExpressionEditorY)
          : FString::Printf(TEXT("Node '%s' moved. %s"), *NodeId, *PlacementWarning),
      Result);
  return true;
}
}
#endif
