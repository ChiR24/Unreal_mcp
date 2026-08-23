#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetMaterialNodeDetails(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_material_node_details")) {
    FString AssetPath, NodeId;
    // The published capability schema spells this `materialPath`, but this handler read only
    // `assetPath`. With additionalProperties:false that made the capability uncallable by any
    // input at all. Prefer the contract spelling and keep `assetPath` as the fallback.
    if ((!Payload->TryGetStringField(TEXT("materialPath"), AssetPath) || AssetPath.IsEmpty()) &&
        (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath' (or 'assetPath')."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("nodeId"), NodeId) || NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
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

    UMaterialExpression *Expr = Material
        ? FindExpressionByIdOrName(Material, NodeId)
        : FindExpressionByIdOrNameInFunction(Function, NodeId);
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
    Result->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
    Result->SetStringField(TEXT("nodeName"), Expr->GetName());
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));

    // Parameter defaults. Without these there is no way to read back what a parameter is
    // actually set to: get_material_info lists names only, and get_node_properties returns
    // nothing, so a written value could never be confirmed.
    if (UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
      Result->SetNumberField(TEXT("scalarDefault"), ScalarParam->DefaultValue);
    } else if (UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>(Expr)) {
      Result->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
      TSharedPtr<FJsonObject> Rgba = McpHandlerUtils::CreateResultObject();
      Rgba->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
      Rgba->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
      Rgba->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
      Rgba->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
      Result->SetObjectField(TEXT("vectorDefault"), Rgba);
    }

    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      Result->SetStringField(TEXT("inputName"), In->InputName.ToString());
      Result->SetStringField(TEXT("inputType"), FunctionInputTypeToString(In->InputType));
      Result->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
      Result->SetNumberField(TEXT("sortPriority"), In->SortPriority);
    } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      Result->SetStringField(TEXT("outputName"), Out->OutputName.ToString());
      Result->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Node details retrieved."), Result);
    return true;
  }

  return false;
}
}
#endif
