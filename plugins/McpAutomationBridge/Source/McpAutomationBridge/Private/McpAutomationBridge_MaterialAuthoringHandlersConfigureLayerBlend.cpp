#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleConfigureLayerBlend(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("configure_layer_blend")) {
    // Configure layer blend by adding layer weight parameters and blend setup
    FString AssetPath;
    // Accept both assetPath and materialPath as parameter names
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), AssetPath) && !AssetPath.IsEmpty()) {
      // Use materialPath
    } else {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' or 'materialPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Parse layers array
    const TArray<TSharedPtr<FJsonValue>> *LayersArray;
    if (!Payload->TryGetArrayField(TEXT("layers"), LayersArray) ||
        LayersArray->Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing or empty 'layers' array."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> CreatedNodeIds;
    int32 BaseX = 0, BaseY = 0;
    Payload->TryGetNumberField(TEXT("x"), BaseX);
    Payload->TryGetNumberField(TEXT("y"), BaseY);

    // For each layer, create a scalar parameter for layer weight
    for (int32 i = 0; i < LayersArray->Num(); ++i) {
      const TSharedPtr<FJsonObject> *LayerObj;
      if (!(*LayersArray)[i]->TryGetObject(LayerObj)) {
        continue;
      }

      FString LayerName;
      if (!(*LayerObj)->TryGetStringField(TEXT("name"), LayerName) ||
          LayerName.IsEmpty()) {
        continue;
      }

      FString BlendType;
      (*LayerObj)->TryGetStringField(TEXT("blendType"), BlendType);

      // Create scalar parameter for layer weight
      UMaterialExpressionScalarParameter *WeightParam =
          NewObject<UMaterialExpressionScalarParameter>(
              Material, UMaterialExpressionScalarParameter::StaticClass(),
              NAME_None, RF_Transactional);

      WeightParam->ParameterName = FName(*LayerName);
      WeightParam->DefaultValue = (i == 0) ? 1.0f : 0.0f; // First layer enabled by default
      WeightParam->MaterialExpressionEditorX = BaseX;
      WeightParam->MaterialExpressionEditorY = BaseY + (i * 150);

#if WITH_EDITORONLY_DATA
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(WeightParam);
#endif

      CreatedNodeIds.Add(MCP_NODE_ID(WeightParam));
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    // Save if requested
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("layerCount"), CreatedNodeIds.Num());

    TArray<TSharedPtr<FJsonValue>> NodeIdArray;
    for (const FString &NodeId : CreatedNodeIds) {
      NodeIdArray.Add(MakeShared<FJsonValueString>(NodeId));
    }
    Result->SetArrayField(TEXT("nodeIds"), NodeIdArray);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Layer blend configured with %d layers."),
                                          CreatedNodeIds.Num()),
                           Result);
    return true;
  }

  // ==========================================================================
  // 8.6 Utilities
  // ==========================================================================

  // --------------------------------------------------------------------------
  // compile_material
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
