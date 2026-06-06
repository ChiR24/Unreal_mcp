#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
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
    if (Val->TryGetString(Id) && !Id.IsEmpty()) FilterNodeIds.Add(Id);
  }
}
bool bFilterConnections = FilterNodeIds.Num() > 0;

// Helper lambda: scan an expression's FExpressionInput properties and emit connections
auto EmitExprConnections = [&](UMaterialExpression *TargetExpr) {
  if (!TargetExpr) return;
  FString TargetId = MCP_NODE_ID(TargetExpr);

  // Iterate all FExpressionInput struct properties via reflection
  for (TFieldIterator<FStructProperty> It(TargetExpr->GetClass()); It; ++It) {
    FStructProperty *StructProp = *It;
    if (!StructProp->Struct || StructProp->Struct->GetFName() != FName(TEXT("ExpressionInput"))) continue;
    FExpressionInput *InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
    if (!InputPtr || !InputPtr->Expression) continue;
    FString SourceId = MCP_NODE_ID(InputPtr->Expression);
    if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;

    TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
    ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
    ConnObj->SetNumberField(TEXT("sourceOutputIndex"), InputPtr->OutputIndex);
    ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
    ConnObj->SetStringField(TEXT("targetInput"), StructProp->GetName());
    ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
  }

  // Custom expression named inputs
  if (UMaterialExpressionCustom *CExpr = Cast<UMaterialExpressionCustom>(TargetExpr)) {
    for (int32 ci = 0; ci < CExpr->Inputs.Num(); ++ci) {
      if (CExpr->Inputs[ci].Input.Expression) {
        FString SourceId = MCP_NODE_ID(CExpr->Inputs[ci].Input.Expression);
        if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;
        TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
        ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
        ConnObj->SetNumberField(TEXT("sourceOutputIndex"), CExpr->Inputs[ci].Input.OutputIndex);
        ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
        ConnObj->SetStringField(TEXT("targetInput"), CExpr->Inputs[ci].InputName.ToString());
        ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
      }
    }
  }

  // MaterialFunctionCall inputs
  if (UMaterialExpressionMaterialFunctionCall *MFC = Cast<UMaterialExpressionMaterialFunctionCall>(TargetExpr)) {
    for (const FFunctionExpressionInput &FI : MFC->FunctionInputs) {
      if (FI.Input.Expression) {
        FString SourceId = MCP_NODE_ID(FI.Input.Expression);
        if (bFilterConnections && !FilterNodeIds.Contains(SourceId) && !FilterNodeIds.Contains(TargetId)) continue;
        TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
        ConnObj->SetStringField(TEXT("sourceNodeId"), SourceId);
        ConnObj->SetNumberField(TEXT("sourceOutputIndex"), FI.Input.OutputIndex);
        ConnObj->SetStringField(TEXT("targetNodeId"), TargetId);
        ConnObj->SetStringField(TEXT("targetInput"), FI.ExpressionInput->InputName.ToString());
        ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
      }
    }
  }
};

for (UMaterialExpression *Expr : AllExpressions) {
  EmitExprConnections(Expr);
}

// Material main pin connections (BaseColor, Normal, etc.)
if (Material) {
#if WITH_EDITORONLY_DATA
  auto EmitMainPin = [&](const FString &PinName, const FExpressionInput &Input) {
    if (Input.Expression) {
      FString SrcId = MCP_NODE_ID(Input.Expression);
      if (bFilterConnections && !FilterNodeIds.Contains(SrcId) && !FilterNodeIds.Contains(TEXT("Main"))) return;
      TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
      ConnObj->SetStringField(TEXT("sourceNodeId"), SrcId);
      ConnObj->SetNumberField(TEXT("sourceOutputIndex"), Input.OutputIndex);
      ConnObj->SetStringField(TEXT("targetNodeId"), TEXT("Main"));
      ConnObj->SetStringField(TEXT("targetInput"), PinName);
      ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
    }
  };
  EmitMainPin(TEXT("BaseColor"), MCP_GET_MATERIAL_INPUT(Material, BaseColor));
  EmitMainPin(TEXT("EmissiveColor"), MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
  EmitMainPin(TEXT("Roughness"), MCP_GET_MATERIAL_INPUT(Material, Roughness));
  EmitMainPin(TEXT("Metallic"), MCP_GET_MATERIAL_INPUT(Material, Metallic));
  EmitMainPin(TEXT("Specular"), MCP_GET_MATERIAL_INPUT(Material, Specular));
  EmitMainPin(TEXT("Normal"), MCP_GET_MATERIAL_INPUT(Material, Normal));
  EmitMainPin(TEXT("Opacity"), MCP_GET_MATERIAL_INPUT(Material, Opacity));
  EmitMainPin(TEXT("OpacityMask"), MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
  EmitMainPin(TEXT("AmbientOcclusion"), MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
  EmitMainPin(TEXT("SubsurfaceColor"), MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
  EmitMainPin(TEXT("WorldPositionOffset"), MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
}

Result->SetArrayField(TEXT("connections"), ConnsArray);
}
}
#endif
