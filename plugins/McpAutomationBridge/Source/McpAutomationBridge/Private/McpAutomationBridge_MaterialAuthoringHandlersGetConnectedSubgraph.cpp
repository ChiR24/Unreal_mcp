#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetConnectedSubgraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_connected_subgraph")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    bool bOrphansOnly = false;
    Payload->TryGetBoolField(TEXT("orphansOnly"), bOrphansOnly);

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    if (NodeId.IsEmpty() && !bOrphansOnly) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' (or set orphansOnly=true)."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // Build bidirectional adjacency
    TMultiMap<UMaterialExpression*, UMaterialExpression*> Adj;
    for (UMaterialExpression *Expr : AllExpr) {
      if (!Expr) continue;
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) {
          Adj.Add(InPtr->Expression, Expr);
          Adj.Add(Expr, InPtr->Expression);
        }
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) { Adj.Add(CI.Input.Expression, Expr); Adj.Add(Expr, CI.Input.Expression); }
        }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) { Adj.Add(FI.Input.Expression, Expr); Adj.Add(Expr, FI.Input.Expression); }
        }
      }
    }

    // Find nodes connected to Material main pins (or MF FunctionOutputs)
    TSet<UMaterialExpression*> OutputConnected;
    TArray<UMaterialExpression*> FloodQueue;
    if (Material) {
#if WITH_EDITORONLY_DATA
      auto SeedMain = [&](const FExpressionInput &Input) {
        if (Input.Expression) { OutputConnected.Add(Input.Expression); FloodQueue.Add(Input.Expression); }
      };
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Roughness));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Metallic));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Specular));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Normal));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, Opacity));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
      SeedMain(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
    } else {
      // For MF, seed from FunctionOutput nodes
      for (UMaterialExpression *Expr : AllExpr) {
        if (Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          OutputConnected.Add(Expr);
          FloodQueue.Add(Expr);
        }
      }
    }
    // Flood fill from output-connected seeds
    int32 FIdx = 0;
    while (FIdx < FloodQueue.Num()) {
      UMaterialExpression *Cur = FloodQueue[FIdx++];
      TArray<UMaterialExpression*> Neighbors;
      Adj.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!OutputConnected.Contains(N)) {
          OutputConnected.Add(N);
          FloodQueue.Add(N);
        }
      }
    }

    if (bOrphansOnly) {
      // Return all nodes NOT connected to any output
      TArray<TSharedPtr<FJsonValue>> OrphansArr;
      for (UMaterialExpression *Expr : AllExpr) {
        if (!Expr) continue;
        if (!OutputConnected.Contains(Expr)) {
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
          Obj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
          Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
          OrphansArr.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetNumberField(TEXT("orphanCount"), OrphansArr.Num());
      Result->SetArrayField(TEXT("orphans"), OrphansArr);
      Bridge->SendAutomationResponse(Socket, RequestId, true,
                             FString::Printf(TEXT("Found %d orphaned node(s)."), OrphansArr.Num()),
                             Result);
      return true;
    }

    // Flood fill from specified nodeId
    UMaterialExpression *SeedExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!SeedExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    TSet<UMaterialExpression*> Island;
    TArray<UMaterialExpression*> IslandQueue;
    Island.Add(SeedExpr);
    IslandQueue.Add(SeedExpr);
    int32 IIdx = 0;
    while (IIdx < IslandQueue.Num()) {
      UMaterialExpression *Cur = IslandQueue[IIdx++];
      TArray<UMaterialExpression*> Neighbors;
      Adj.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!Island.Contains(N)) {
          Island.Add(N);
          IslandQueue.Add(N);
        }
      }
    }

    TArray<TSharedPtr<FJsonValue>> NodesArr;
    for (UMaterialExpression *Expr : Island) {
      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
      Obj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
      Obj->SetStringField(TEXT("desc"), Expr->GetDescription());
      Obj->SetBoolField(TEXT("connectedToOutput"), OutputConnected.Contains(Expr));
      NodesArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("islandSize"), NodesArr.Num());
    Result->SetArrayField(TEXT("nodes"), NodesArr);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Subgraph contains %d node(s)."), NodesArr.Num()),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_material_node - Generic node adder
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
