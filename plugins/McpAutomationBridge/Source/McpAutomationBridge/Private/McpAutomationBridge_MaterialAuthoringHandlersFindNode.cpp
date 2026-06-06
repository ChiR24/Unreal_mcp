#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
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

    // Build connection count map for each expression
    TMap<FGuid, int32> ConnectionCountMap;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;
      int32 Count = 0;
      // Count input connections via reflection
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) Count++;
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) { if (CI.Input.Expression) Count++; }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) { if (FI.Input.Expression) Count++; }
      }
      // Count output connections (other exprs referencing this one)
      for (UMaterialExpression *Other : Exprs) {
        if (!Other || Other == Expr) continue;
        for (TFieldIterator<FStructProperty> It2(Other->GetClass()); It2; ++It2) {
          FStructProperty *SP2 = *It2;
          if (!SP2->Struct || SP2->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
          FExpressionInput *InPtr2 = SP2->ContainerPtrToValuePtr<FExpressionInput>(Other);
          if (InPtr2 && InPtr2->Expression == Expr) Count++;
        }
      }
      ConnectionCountMap.Add(Expr->MaterialExpressionGuid, Count);
    }

    TSet<FGuid> SeenIds;
    TArray<TSharedPtr<FJsonValue>> Matches;
    for (UMaterialExpression *Expr : Exprs) {
      if (!Expr) continue;

      // Deduplicate by GUID
      if (SeenIds.Contains(Expr->MaterialExpressionGuid)) continue;

      FString ClassName = Expr->GetClass()->GetName();

      // Type match (substring)
      if (!SearchType.IsEmpty() && !ClassName.Contains(SearchType)) continue;

      // Name match
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

  // --------------------------------------------------------------------------
  // get_node_connections — get connections for a node with graph traversal
  //   direction: "inputs"|"outputs"|"both" (default "both")
  //   depth: int (default 1, -1 = unlimited)
  //   upstream: bool — walk backward to all sources
  //   downstream: bool — walk forward to all consumers
  //   Returns flattened list with "hop" field indicating distance from origin
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
