#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetNodeChain(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_node_chain")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString StartNodeId, EndNodeId, EndPin;
    Payload->TryGetStringField(TEXT("startNodeId"), StartNodeId);
    Payload->TryGetStringField(TEXT("endNodeId"), EndNodeId);
    Payload->TryGetStringField(TEXT("endPin"), EndPin);

    if (StartNodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'startNodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (EndNodeId.IsEmpty() && EndPin.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'endNodeId' or 'endPin'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *StartExpr = FIND_EXPR_IN_HOST(StartNodeId);
    if (!StartExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Start node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // Build downstream adjacency: Source → list of Targets
    TMultiMap<UMaterialExpression*, UMaterialExpression*> Downstream;
    for (UMaterialExpression *Expr : AllExpr) {
      if (!Expr) continue;
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (InPtr && InPtr->Expression) {
          Downstream.Add(InPtr->Expression, Expr);
        }
      }
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) Downstream.Add(CI.Input.Expression, Expr);
        }
      }
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) Downstream.Add(FI.Input.Expression, Expr);
        }
      }
    }

    // Check if endPin refers to a Material main pin
    UMaterialExpression *EndExpr = nullptr;
    bool bEndIsMainPin = false;
    if (!EndNodeId.IsEmpty() && EndNodeId != TEXT("Main")) {
      EndExpr = FIND_EXPR_IN_HOST(EndNodeId);
    }
    if (!EndPin.IsEmpty() || EndNodeId == TEXT("Main")) {
      bEndIsMainPin = true;
    }

    // BFS from Start downstream
    TMap<UMaterialExpression*, UMaterialExpression*> Parent;
    TArray<UMaterialExpression*> BFSQueue;
    BFSQueue.Add(StartExpr);
    Parent.Add(StartExpr, nullptr);
    bool bPathFound = false;
    UMaterialExpression *PathEnd = nullptr;

    int32 Idx = 0;
    while (Idx < BFSQueue.Num()) {
      UMaterialExpression *Cur = BFSQueue[Idx++];

      // Check if we reached the end
      if (EndExpr && Cur == EndExpr) { bPathFound = true; PathEnd = Cur; break; }
      if (bEndIsMainPin && Material) {
#if WITH_EDITORONLY_DATA
        // Check if Cur feeds any main pin
        auto IsMainTarget = [&](const FExpressionInput &Input) { return Input.Expression == Cur; };
        if ((!EndPin.IsEmpty() && EndPin == TEXT("BaseColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, BaseColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("EmissiveColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Roughness") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Roughness))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Metallic") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Metallic))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Normal") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Normal))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Opacity") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Opacity))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("OpacityMask") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, OpacityMask))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("AmbientOcclusion") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("SubsurfaceColor") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("WorldPositionOffset") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset))) ||
            (!EndPin.IsEmpty() && EndPin == TEXT("Specular") && IsMainTarget(MCP_GET_MATERIAL_INPUT(Material, Specular)))) {
          bPathFound = true; PathEnd = Cur; break;
        }
#endif
      }

      // Enqueue downstream neighbors
      TArray<UMaterialExpression*> Neighbors;
      Downstream.MultiFind(Cur, Neighbors);
      for (UMaterialExpression *N : Neighbors) {
        if (!Parent.Contains(N)) {
          Parent.Add(N, Cur);
          BFSQueue.Add(N);
        }
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (bPathFound && PathEnd) {
      // Reconstruct path
      TArray<UMaterialExpression*> Path;
      UMaterialExpression *Walk = PathEnd;
      while (Walk) {
        Path.Insert(Walk, 0);
        UMaterialExpression **P = Parent.Find(Walk);
        Walk = (P && *P) ? *P : nullptr;
      }
      TArray<TSharedPtr<FJsonValue>> ChainArr;
      for (int32 i = 0; i < Path.Num(); ++i) {
        TSharedPtr<FJsonObject> N = MakeShared<FJsonObject>();
        N->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Path[i]));
        N->SetStringField(TEXT("type"), Path[i]->GetClass()->GetName());
        N->SetStringField(TEXT("desc"), Path[i]->GetDescription());
        N->SetNumberField(TEXT("step"), i);
        ChainArr.Add(MakeShared<FJsonValueObject>(N));
      }
      if (bEndIsMainPin) {
        TSharedPtr<FJsonObject> MainNode = MakeShared<FJsonObject>();
        MainNode->SetStringField(TEXT("nodeId"), TEXT("Main"));
        MainNode->SetStringField(TEXT("type"), TEXT("MaterialOutput"));
        MainNode->SetStringField(TEXT("desc"), EndPin.IsEmpty() ? TEXT("Main") : EndPin);
        MainNode->SetNumberField(TEXT("step"), Path.Num());
        ChainArr.Add(MakeShared<FJsonValueObject>(MainNode));
      }
      Result->SetBoolField(TEXT("pathFound"), true);
      Result->SetNumberField(TEXT("length"), ChainArr.Num());
      Result->SetArrayField(TEXT("chain"), ChainArr);
    } else {
      Result->SetBoolField(TEXT("pathFound"), false);
      Result->SetStringField(TEXT("message"), TEXT("No path found between the specified nodes."));
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           bPathFound ? TEXT("Signal path found.") : TEXT("No path found."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_connected_subgraph — island detection + orphansOnly
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
