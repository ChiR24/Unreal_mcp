#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleDeleteNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("delete_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    // Accept single nodeId or array of nodeIds
    TArray<FString> NodeIds;
    FString SingleId;
    if (Payload->TryGetStringField(TEXT("nodeId"), SingleId) && !SingleId.IsEmpty()) {
      NodeIds.Add(SingleId);
    }
    const TArray<TSharedPtr<FJsonValue>> *IdsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("nodeIds"), IdsArr) && IdsArr) {
      for (const auto &Val : *IdsArr) {
        FString Id;
        if (McpHandlerUtils::TryGetJsonValueString(Val, Id) && !Id.IsEmpty()) NodeIds.Add(Id);
      }
    }
    if (NodeIds.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'nodeIds'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    TArray<FString> Removed, NotFound;
    for (const FString &NId : NodeIds) {
      UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NId);
      if (!Expr) { NotFound.Add(NId); continue; }

      // Auto-disconnect: clear all references to this node from other expressions
      for (UMaterialExpression *Other : AllExpr) {
        if (!Other || Other == Expr) continue;
        ForEachExpressionInput(Other, [&](FExpressionInput &Input, const FString &) {
          if (Input.Expression == Expr) { Input.Expression = nullptr; Input.OutputIndex = 0; }
        });
      }

      // Clear Material main pin references
      if (Material) {
        ForEachMainMaterialInput(Material, [&](const TCHAR *, FExpressionInput &Input) {
          if (Input.Expression == Expr) { Input.Expression = nullptr; Input.OutputIndex = 0; }
        });
      }

      AllExpr.Remove(Expr); // what RemoveExpression does on 5.1+, the array itself on 5.0
      Removed.Add(NId);
    }

    // An id that matched nothing used to vanish from the reply, so deleting a
    // misspelled node answered "Deleted 0 node(s)" as a success.
    if (Removed.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("No node matched: %s. Read the ids with get_material_info or find_node."), *FString::Join(NotFound, TEXT(", "))),
          TEXT("NOT_FOUND"));
      return true;
    }
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> RemovedArr;
    for (const FString &R : Removed) {
      RemovedArr.Add(MakeShared<FJsonValueString>(R));
    }
    Result->SetArrayField(TEXT("removed"), RemovedArr);
    Result->SetNumberField(TEXT("removedCount"), Removed.Num());
    TArray<TSharedPtr<FJsonValue>> NotFoundArr;
    for (const FString &Missing : NotFound) { NotFoundArr.Add(MakeShared<FJsonValueString>(Missing)); }
    Result->SetArrayField(TEXT("notFound"), NotFoundArr);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Deleted %d node(s)."), Removed.Num()),
                           Result);
    return true;
  }

  return false;
}
}
