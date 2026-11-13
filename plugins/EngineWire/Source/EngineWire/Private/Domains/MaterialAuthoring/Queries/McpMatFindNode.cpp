#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleFindNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("find_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString SearchType, SearchName;
    Payload->TryGetStringField(TEXT("nodeType"), SearchType);
    // The contract declares nodeName (this used to read only `name`, which the gateway refuses).
    Payload->TryGetStringField(TEXT("nodeName"), SearchName);

    if (SearchType.IsEmpty() && SearchName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Provide at least 'nodeType' or 'nodeName' to search."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& Exprs = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // One pass over every input: a wire counts for the node it enters and for the node it leaves. Keyed by the
    // expression, as two nodes can share a GUID (a pasted or duplicated node), and the second was left out.
    TMap<const UMaterialExpression*, int32> ConnectionCountMap;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;
      ForEachExpressionInput(Expr, [&](FExpressionInput &Input, const FString &) {
        if (Input.Expression) {
          ++ConnectionCountMap.FindOrAdd(Expr);
          ++ConnectionCountMap.FindOrAdd(static_cast<const UMaterialExpression*>(Input.Expression));
        }
      });
    }

    TArray<TSharedPtr<FJsonValue>> Matches;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;

      FString ClassName = Expr->GetClass()->GetName();

      if (!SearchType.IsEmpty() && !ClassName.Contains(SearchType)) continue;

      if (!SearchName.IsEmpty()) {
        bool bNameMatch = false;
        // Every parameter kind by its ParameterName: a texture parameter is no UMaterialExpressionParameter.
        if (Expr->HasAParameterName()) {
          bNameMatch = Expr->GetParameterName().ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          bNameMatch = FI->InputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          bNameMatch = FO->OutputName.ToString().Contains(SearchName);
        } else if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
          bNameMatch = CE->Description.Contains(SearchName) || CE->Code.Contains(SearchName);
        }
        // The node id (object name) matches too, so a name from an earlier reply finds its node.
        bNameMatch = bNameMatch || MCP_NODE_ID(Expr).Contains(SearchName);
        // Both filters must hold; a type match used to override a name miss.
        if (!bNameMatch) continue;
      }

      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
      Obj->SetStringField(TEXT("type"), ClassName);
      Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
      Obj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
      Obj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
      const int32 *CC = ConnectionCountMap.Find(Expr);
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
