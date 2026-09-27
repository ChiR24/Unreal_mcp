#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleAddMaterialNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_material_node")) {
    // materialPath or assetPath, sanitized; Material or Function; HostOuter; X/Y.
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();
    const FString NodeType = McpGetFirstStringField(Payload, {TEXT("nodeType"), TEXT("type")});
    if (NodeType.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeType'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // A node type is a MaterialExpression class name without its prefix
    // ("Lerp" -> MaterialExpressionLinearInterpolate via the alias below) or a
    // full class name/path. Aliases cover the short spellings whose class name
    // differs.
    static const TMap<FString, FString> Aliases = {
        {TEXT("ConstantVectorParameter"), TEXT("VectorParameter")},
        {TEXT("ConstantScalarParameter"), TEXT("ScalarParameter")},
        {TEXT("Float"), TEXT("Constant")}, {TEXT("Scalar"), TEXT("Constant")},
        {TEXT("ConstantVector"), TEXT("Constant3Vector")}, {TEXT("Color"), TEXT("Constant3Vector")},
        {TEXT("Vector3"), TEXT("Constant3Vector")}, {TEXT("Lerp"), TEXT("LinearInterpolate")},
        {TEXT("TexCoord"), TEXT("TextureCoordinate")}, {TEXT("VertexNormal"), TEXT("VertexNormalWS")},
        {TEXT("ReflectionVector"), TEXT("ReflectionVectorWS")}, {TEXT("StaticSwitch"), TEXT("StaticSwitchParameter")}};
    const FString* Alias = Aliases.Find(NodeType);
    const FString TypeName = Alias ? *Alias : NodeType;
    UClass *ExpressionClass = ResolveClassByName(TEXT("MaterialExpression") + TypeName);
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
      ExpressionClass = ResolveClassByName(TypeName);
    }
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
      Bridge->SendAutomationError(
          Socket, RequestId,
          FString::Printf(
              TEXT("Unknown node type: %s. Use a MaterialExpression class name without its prefix "
                   "(e.g. 'Lerp', 'TextureSample', 'ScalarParameter') or the full class name "
                   "(e.g. 'MaterialExpressionLinearInterpolate')."),
              *NodeType),
          TEXT("UNKNOWN_TYPE"));
      return true;
    }

    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        HostOuter, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
      return true;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, NewExpr);

    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName) && !ParamName.IsEmpty()) {
      if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    // Apply the requested default value. Previously only `name` was honoured, so
    // a Constant3Vector/Color requested with defaultValue stayed at (0,0,0) while
    // the response reported success. Accepts [r,g,b(,a)] arrays and {r,g,b,a}
    // objects for colour-style nodes, and a plain number for scalar nodes.
    if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpr)) {
      if (Payload->HasField(TEXT("defaultValue"))) {
        Const3->Constant = ExtractLinearColorField(Payload, TEXT("defaultValue"), FLinearColor::Black);
        Const3->PostEditChange();
      }
    } else if (UMaterialExpressionConstant *ConstScalar = Cast<UMaterialExpressionConstant>(NewExpr)) {
      double ScalarDefault = 0.0;
      if (Payload->TryGetNumberField(TEXT("defaultValue"), ScalarDefault)) {
        ConstScalar->R = static_cast<float>(ScalarDefault);
        ConstScalar->PostEditChange();
      }
    }

    HostOuter->PostEditChange();
    HostOuter->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(NewExpr));
    // Placement telemetry was emitted only by the parameter-adding variants, so
    // the documented overlappingNodes / placementWarning detection could never
    // fire for the node kinds a caller stacks in a loop.
    AddMaterialNodePlacementFields(Result, Material, NewExpr);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Result->SetBoolField(TEXT("nodeAdded"), true);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material node '%s' added."), *NodeType), Result);
    return true;
  }

  return false;
}
}
