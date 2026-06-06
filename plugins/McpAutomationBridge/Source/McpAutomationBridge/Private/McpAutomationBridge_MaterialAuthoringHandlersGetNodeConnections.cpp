#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetNodeConnections(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_node_connections")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *StartExpr = FIND_EXPR_IN_HOST(NodeId);
    if (!StartExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Parse options
    FString Direction;
    Payload->TryGetStringField(TEXT("direction"), Direction);
    if (Direction.IsEmpty()) Direction = TEXT("both");
    bool bWantInputs  = (Direction == TEXT("inputs")  || Direction == TEXT("both"));
    bool bWantOutputs = (Direction == TEXT("outputs") || Direction == TEXT("both"));

    double DepthD = 1.0;
    Payload->TryGetNumberField(TEXT("depth"), DepthD);
    int32 MaxDepth = (int32)DepthD;

    bool bUpstream = false, bDownstream = false;
    Payload->TryGetBoolField(TEXT("upstream"), bUpstream);
    Payload->TryGetBoolField(TEXT("downstream"), bDownstream);
    // upstream/downstream override direction+depth
    if (bUpstream) { bWantInputs = true; bWantOutputs = false; if (MaxDepth > 0) MaxDepth = 9999; }
    if (bDownstream) { bWantOutputs = true; bWantInputs = false; if (MaxDepth > 0) MaxDepth = 9999; }
    if (bUpstream && bDownstream) { bWantInputs = true; bWantOutputs = true; }
    if (MaxDepth == -1) MaxDepth = 9999;

    auto& AllExpr = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);

    // --- Build adjacency: for each expression, find its input sources ---
    // InputSourcesOf[Expr] = list of {SourceExpr, OutputIndex, PinName}
    struct FEdge {
      UMaterialExpression *Source;
      UMaterialExpression *Target;
      int32 OutputIndex;
      FString PinName;
    };
    TArray<FEdge> AllEdges;

    auto CollectInputEdges = [&](UMaterialExpression *Expr) {
      if (!Expr) return;
      // Reflection-based inputs
      for (TFieldIterator<FStructProperty> It(Expr->GetClass()); It; ++It) {
        FStructProperty *SP = *It;
        if (!SP->Struct || SP->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
        FExpressionInput *InPtr = SP->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (!InPtr || !InPtr->Expression) continue;
        AllEdges.Add({InPtr->Expression, Expr, InPtr->OutputIndex, SP->GetName()});
      }
      // Custom expression named inputs
      if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
        for (const FCustomInput &CI : CE->Inputs) {
          if (CI.Input.Expression) {
            AllEdges.Add({CI.Input.Expression, Expr, CI.Input.OutputIndex, CI.InputName.ToString()});
          }
        }
      }
      // MF call inputs
      if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
          if (FI.Input.Expression) {
            AllEdges.Add({FI.Input.Expression, Expr, FI.Input.OutputIndex, FI.ExpressionInput->InputName.ToString()});
          }
        }
      }
    };

    for (UMaterialExpression *Expr : AllExpr) {
      CollectInputEdges(Expr);
    }

    // Material main pin edges
    TArray<FEdge> MainPinEdges;
    if (Material) {
#if WITH_EDITORONLY_DATA
      auto AddMainEdge = [&](const FString &PinName, const FExpressionInput &Input) {
        if (Input.Expression) {
          MainPinEdges.Add({Input.Expression, nullptr, Input.OutputIndex, PinName});
        }
      };
      AddMainEdge(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(Material, BaseColor));
      AddMainEdge(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
      AddMainEdge(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(Material, Roughness));
      AddMainEdge(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(Material, Metallic));
      AddMainEdge(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(Material, Specular));
      AddMainEdge(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(Material, Normal));
      AddMainEdge(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(Material, Opacity));
      AddMainEdge(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
      AddMainEdge(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
      AddMainEdge(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
      AddMainEdge(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
    }

    // --- BFS traversal ---
    struct FNodeHop { UMaterialExpression *Expr; int32 Hop; };
    TArray<TSharedPtr<FJsonValue>> ResultConns;
    TSet<FGuid> Visited;
    Visited.Add(StartExpr->MaterialExpressionGuid);

    TArray<FNodeHop> Queue;
    Queue.Add({StartExpr, 0});
    int32 QueueIdx = 0;

    while (QueueIdx < Queue.Num()) {
      FNodeHop Current = Queue[QueueIdx++];
      if (Current.Hop >= MaxDepth) continue;

      // Walk upstream (inputs): edges where Current is the Target
      if (bWantInputs) {
        for (const FEdge &E : AllEdges) {
          if (E.Target != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(E.Source));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("input"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
          if (!Visited.Contains(E.Source->MaterialExpressionGuid)) {
            Visited.Add(E.Source->MaterialExpressionGuid);
            Queue.Add({E.Source, Current.Hop + 1});
          }
        }
      }

      // Walk downstream (outputs): edges where Current is the Source
      if (bWantOutputs) {
        for (const FEdge &E : AllEdges) {
          if (E.Source != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), MCP_NODE_ID(E.Target));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("output"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
          if (!Visited.Contains(E.Target->MaterialExpressionGuid)) {
            Visited.Add(E.Target->MaterialExpressionGuid);
            Queue.Add({E.Target, Current.Hop + 1});
          }
        }
        // Main pin outputs
        for (const FEdge &E : MainPinEdges) {
          if (E.Source != Current.Expr) continue;
          TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
          Obj->SetStringField(TEXT("sourceNodeId"), MCP_NODE_ID(Current.Expr));
          Obj->SetNumberField(TEXT("sourceOutputIndex"), E.OutputIndex);
          Obj->SetStringField(TEXT("targetNodeId"), TEXT("Main"));
          Obj->SetStringField(TEXT("targetInput"), E.PinName);
          Obj->SetNumberField(TEXT("hop"), Current.Hop + 1);
          Obj->SetStringField(TEXT("direction"), TEXT("output"));
          ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(StartExpr));
    Result->SetStringField(TEXT("type"), StartExpr->GetClass()->GetName());
    Result->SetNumberField(TEXT("connectionCount"), ResultConns.Num());
    Result->SetArrayField(TEXT("connections"), ResultConns);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Node connections retrieved."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_node_properties — read all properties of a specific node
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
