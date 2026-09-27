#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

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
      ForEachExpressionInput(Expr, [&](FExpressionInput &Input, const FString &PinName) {
        if (Input.Expression) AllEdges.Add({Input.Expression, Expr, Input.OutputIndex, PinName});
      });
    };

    for (UMaterialExpression *Expr : AllExpr) {
      CollectInputEdges(Expr);
    }

    // Material main pin edges
    TArray<FEdge> MainPinEdges;
    if (Material) {
      auto AddMainEdge = [&](const FString &PinName, const FExpressionInput &Input) {
        if (Input.Expression) {
          MainPinEdges.Add({Input.Expression, nullptr, Input.OutputIndex, PinName});
        }
      };
      ForEachMainMaterialInput(Material, AddMainEdge);
    }

    struct FNodeHop { UMaterialExpression *Expr; int32 Hop; };
    TArray<TSharedPtr<FJsonValue>> ResultConns;
    auto EmitEdge = [&ResultConns](const FString &SourceId, int32 OutputIndex, const FString &TargetId,
                                   const FString &PinName, int32 Hop, const TCHAR *Direction) {
      TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
      Obj->SetStringField(TEXT("sourceNodeId"), SourceId);
      Obj->SetNumberField(TEXT("sourceOutputIndex"), OutputIndex);
      Obj->SetStringField(TEXT("targetNodeId"), TargetId);
      Obj->SetStringField(TEXT("targetInput"), PinName);
      Obj->SetNumberField(TEXT("hop"), Hop);
      Obj->SetStringField(TEXT("direction"), Direction);
      ResultConns.Add(MakeShared<FJsonValueObject>(Obj));
    };
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
          EmitEdge(MCP_NODE_ID(E.Source), E.OutputIndex, MCP_NODE_ID(Current.Expr), E.PinName, Current.Hop + 1, TEXT("input"));
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
          EmitEdge(MCP_NODE_ID(Current.Expr), E.OutputIndex, MCP_NODE_ID(E.Target), E.PinName, Current.Hop + 1, TEXT("output"));
          if (!Visited.Contains(E.Target->MaterialExpressionGuid)) {
            Visited.Add(E.Target->MaterialExpressionGuid);
            Queue.Add({E.Target, Current.Hop + 1});
          }
        }
        // Main pin outputs
        for (const FEdge &E : MainPinEdges) {
          if (E.Source != Current.Expr) continue;
          EmitEdge(MCP_NODE_ID(Current.Expr), E.OutputIndex, TEXT("Main"), E.PinName, Current.Hop + 1, TEXT("output"));
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

  return false;
}
}
