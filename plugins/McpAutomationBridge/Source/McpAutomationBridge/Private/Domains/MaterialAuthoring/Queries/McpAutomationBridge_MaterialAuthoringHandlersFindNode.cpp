#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleFindNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("find_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString SearchType, SearchName;
    Payload->TryGetStringField(TEXT("nodeType"), SearchType);
    Payload->TryGetStringField(TEXT("name"), SearchName);

    if (SearchType.IsEmpty() && SearchName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Provide at least 'nodeType' or 'name' to search."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& Exprs = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    TMap<FGuid, int32> ConnectionCountMap;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;
      int32 Count = 0;
      // Inputs wired into this node, then other nodes' inputs wired from it.
      ForEachExpressionInput(Expr, [&](FExpressionInput &Input, const FString &) { Count += Input.Expression ? 1 : 0; });
      for (UMaterialExpression *Other : Exprs) {
        if (!Other || Other == Expr) continue;
        ForEachExpressionInput(Other, [&](FExpressionInput &Input, const FString &) { Count += Input.Expression == Expr ? 1 : 0; });
      }
      ConnectionCountMap.Add(Expr->MaterialExpressionGuid, Count);
    }

    TSet<FGuid> SeenIds;
    TArray<TSharedPtr<FJsonValue>> Matches;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;

      if (SeenIds.Contains(Expr->MaterialExpressionGuid)) continue;

      FString ClassName = Expr->GetClass()->GetName();

      if (!SearchType.IsEmpty() && !ClassName.Contains(SearchType)) continue;

      if (!SearchName.IsEmpty()) {
        bool bNameMatch = false;
        if (UMaterialExpressionParameter *P = Cast<UMaterialExpressionParameter>(Expr)) {
          bNameMatch = P->ParameterName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          bNameMatch = FI->InputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          bNameMatch = FO->OutputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
          bNameMatch = CE->Description.Contains(SearchName) || CE->Code.Contains(SearchName);
        }
        if (!bNameMatch && SearchType.IsEmpty()) continue;
      }

      SeenIds.Add(Expr->MaterialExpressionGuid);

      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
      Obj->SetStringField(TEXT("type"), ClassName);
      Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
      Obj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
      Obj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
      int32 *CC = ConnectionCountMap.Find(Expr->MaterialExpressionGuid);
      Obj->SetNumberField(TEXT("connectionCount"), CC ? *CC : 0);
      Matches.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("matchCount"), Matches.Num());
    Result->SetArrayField(TEXT("nodes"), Matches);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Found %d matching node(s)."), Matches.Num()),
                           Result);
    return true;
  }

  return false;
}
}
