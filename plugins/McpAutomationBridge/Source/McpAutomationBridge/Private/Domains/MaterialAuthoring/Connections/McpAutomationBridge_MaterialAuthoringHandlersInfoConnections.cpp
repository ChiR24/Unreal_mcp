#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
void AppendMaterialInfoConnections(UMaterial* Material, UMaterialFunction* Function, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
  auto& AllExpressions = Material
      ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
      : MCP_GET_FUNCTION_EXPRESSIONS(Function);
  TArray<TSharedPtr<FJsonValue>> ConnsArray;
  
  // Optional nodeId / nodeIds filter for connections
  TSet<FString> FilterNodeIds;
  FString SingleNodeId;
  if (Payload->TryGetStringField(TEXT("nodeId"), SingleNodeId) && !SingleNodeId.IsEmpty()) {
    FilterNodeIds.Add(SingleNodeId);
  }
  const TArray<TSharedPtr<FJsonValue>> *NodeIdsArr = nullptr;
  if (Payload->TryGetArrayField(TEXT("nodeIds"), NodeIdsArr) && NodeIdsArr) {
    for (const auto &Val : *NodeIdsArr) {
      FString Id;
      if (McpHandlerUtils::TryGetJsonValueString(Val, Id) && !Id.IsEmpty()) FilterNodeIds.Add(Id);
    }
  }
  bool bFilterConnections = FilterNodeIds.Num() > 0;
  
  // One edge per wired input; with a filter, only edges touching a listed node ("Main" = the material pins).
  auto EmitConn = [&](const FExpressionInput &Input, const FString &TargetId, const FString &PinName) {
    if (!Input.Expression) return;
    const FString SourceId = MCP_NODE_ID(Input.Expression);
    if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) return;
    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
    ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
    ConnObj->SetNumberField(TEXT("sourceOutputIndex"), Input.OutputIndex);
    ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
    ConnObj->SetStringField(TEXT("targetInput"), PinName);
    ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
  };
  for (UMaterialExpression *Expr : AllExpressions) {
    if (Expr) {
      const FString TargetId = MCP_NODE_ID(Expr);
      ForEachExpressionInput(Expr, [&](FExpressionInput &Input, const FString &PinName) { EmitConn(Input, TargetId, PinName); });
    }
  }
  if (Material) {
    ForEachMainMaterialInput(Material, [&](const TCHAR *PinName, FExpressionInput &Input) { EmitConn(Input, TEXT("Main"), PinName); });
  }
  
  Result->SetArrayField(TEXT("connections"), ConnsArray);
}
}
