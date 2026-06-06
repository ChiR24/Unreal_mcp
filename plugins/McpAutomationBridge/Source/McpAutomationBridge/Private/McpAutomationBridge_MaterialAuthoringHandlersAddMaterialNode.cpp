#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddMaterialNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_material_node")) {
    FString AssetPath, NodeType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) || NodeType.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeType'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate asset path using SanitizeProjectRelativePath
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                  : static_cast<UObject*>(Function);

    // Get position from payload
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // Resolve the expression class based on nodeType
    UClass *ExpressionClass = nullptr;
    if (NodeType == TEXT("TextureSample"))
      ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else if (NodeType == TEXT("VectorParameter") || NodeType == TEXT("ConstantVectorParameter"))
      ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType == TEXT("ScalarParameter") || NodeType == TEXT("ConstantScalarParameter"))
      ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (NodeType == TEXT("Add"))
      ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType == TEXT("Multiply"))
      ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType == TEXT("Constant") || NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))
      ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("ConstantVector") ||
             NodeType == TEXT("Color") || NodeType == TEXT("Vector3"))
      ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
    else if (NodeType == TEXT("Lerp") || NodeType == TEXT("LinearInterpolate"))
      ExpressionClass = UMaterialExpressionLinearInterpolate::StaticClass();
    else if (NodeType == TEXT("Divide"))
      ExpressionClass = UMaterialExpressionDivide::StaticClass();
    else if (NodeType == TEXT("Subtract"))
      ExpressionClass = UMaterialExpressionSubtract::StaticClass();
    else if (NodeType == TEXT("Power"))
      ExpressionClass = UMaterialExpressionPower::StaticClass();
    else if (NodeType == TEXT("Clamp"))
      ExpressionClass = UMaterialExpressionClamp::StaticClass();
    else if (NodeType == TEXT("Frac"))
      ExpressionClass = UMaterialExpressionFrac::StaticClass();
    else if (NodeType == TEXT("OneMinus"))
      ExpressionClass = UMaterialExpressionOneMinus::StaticClass();
    else if (NodeType == TEXT("Panner"))
      ExpressionClass = UMaterialExpressionPanner::StaticClass();
    else if (NodeType == TEXT("TextureCoordinate") || NodeType == TEXT("TexCoord"))
      ExpressionClass = UMaterialExpressionTextureCoordinate::StaticClass();
    else if (NodeType == TEXT("ComponentMask"))
      ExpressionClass = UMaterialExpressionComponentMask::StaticClass();
    else if (NodeType == TEXT("DotProduct"))
      ExpressionClass = UMaterialExpressionDotProduct::StaticClass();
    else if (NodeType == TEXT("CrossProduct"))
      ExpressionClass = UMaterialExpressionCrossProduct::StaticClass();
    else if (NodeType == TEXT("Desaturation"))
      ExpressionClass = UMaterialExpressionDesaturation::StaticClass();
    else if (NodeType == TEXT("Fresnel"))
      ExpressionClass = UMaterialExpressionFresnel::StaticClass();
    else if (NodeType == TEXT("Noise"))
      ExpressionClass = UMaterialExpressionNoise::StaticClass();
    else if (NodeType == TEXT("WorldPosition"))
      ExpressionClass = UMaterialExpressionWorldPosition::StaticClass();
    else if (NodeType == TEXT("VertexNormalWS") || NodeType == TEXT("VertexNormal"))
      ExpressionClass = UMaterialExpressionVertexNormalWS::StaticClass();
    else if (NodeType == TEXT("ReflectionVectorWS") || NodeType == TEXT("ReflectionVector"))
      ExpressionClass = UMaterialExpressionReflectionVectorWS::StaticClass();
    else if (NodeType == TEXT("PixelDepth"))
      ExpressionClass = UMaterialExpressionPixelDepth::StaticClass();
    else if (NodeType == TEXT("AppendVector"))
      ExpressionClass = UMaterialExpressionAppendVector::StaticClass();
    else if (NodeType == TEXT("If"))
      ExpressionClass = UMaterialExpressionIf::StaticClass();
    else if (NodeType == TEXT("MaterialFunctionCall"))
      ExpressionClass = UMaterialExpressionMaterialFunctionCall::StaticClass();
    else if (NodeType == TEXT("FunctionInput"))
      ExpressionClass = UMaterialExpressionFunctionInput::StaticClass();
    else if (NodeType == TEXT("FunctionOutput"))
      ExpressionClass = UMaterialExpressionFunctionOutput::StaticClass();
    else if (NodeType == TEXT("Custom"))
      ExpressionClass = UMaterialExpressionCustom::StaticClass();
    else if (NodeType == TEXT("StaticSwitchParameter") || NodeType == TEXT("StaticSwitch"))
      ExpressionClass = UMaterialExpressionStaticSwitchParameter::StaticClass();
    else if (NodeType == TEXT("TextureSampleParameter2D"))
      ExpressionClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
    else {
      // Try to resolve by full class path or with MaterialExpression prefix
      ExpressionClass = ResolveClassByName(NodeType);
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
      }
      if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        Bridge->SendAutomationError(
            Socket, RequestId,
            FString::Printf(
                TEXT("Unknown node type: %s. Available types: TextureSample, VectorParameter, "
                     "ScalarParameter, Add, Multiply, Constant, Constant3Vector, Color, Lerp, "
                     "Divide, Subtract, Power, Clamp, Frac, OneMinus, Panner, TextureCoordinate, "
                     "ComponentMask, DotProduct, CrossProduct, Desaturation, Fresnel, Noise, "
                     "WorldPosition, VertexNormalWS, ReflectionVectorWS, PixelDepth, AppendVector, "
                     "If, MaterialFunctionCall, FunctionInput, FunctionOutput, Custom, "
                     "StaticSwitchParameter, TextureSampleParameter2D. Or use full class name "
                     "like 'MaterialExpressionLerp'."),
                *NodeType),
            TEXT("UNKNOWN_TYPE"));
        return true;
      }
    }

    // Create the expression
    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        HostOuter, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
      return true;
    }

    // Set editor position
    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    // Add to the host's expression collection (Material or MaterialFunction)
#if WITH_EDITORONLY_DATA
    AddExpressionToContainer(Material, Function, NewExpr);
#endif

    // If parameter node, set the parameter name
    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName) && !ParamName.IsEmpty()) {
      if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    HostOuter->PostEditChange();
    HostOuter->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(NewExpr));
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Result->SetBoolField(TEXT("nodeAdded"), true);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material node '%s' added."), *NodeType), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // remove_material_node
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
