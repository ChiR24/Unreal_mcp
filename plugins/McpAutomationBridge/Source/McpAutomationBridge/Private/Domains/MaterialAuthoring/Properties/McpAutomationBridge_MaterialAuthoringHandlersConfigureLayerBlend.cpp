#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleConfigureLayerBlend(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("configure_layer_blend")) {
    FString AssetPath;
    // Accept both assetPath and materialPath as parameter names
    if (Payload->TryGetStringField(TEXT("assetPath"), AssetPath) && !AssetPath.IsEmpty()) {
    } else if (Payload->TryGetStringField(TEXT("materialPath"), AssetPath) && !AssetPath.IsEmpty()) {
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

    // Dogfood #207: a landscape layer blend is one LandscapeLayerBlend expression whose
    // FLayerBlendInput entries name the layers; scalar weight parameters are not a layer blend.
    FString DefaultBlendType;
    Payload->TryGetStringField(TEXT("blendType"), DefaultBlendType);
    auto ParseBlendType = [](const FString& Text) {
      if (Text.Contains(TEXT("Height"), ESearchCase::IgnoreCase)) { return LB_HeightBlend; }
      if (Text.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase)) { return LB_AlphaBlend; }
      return LB_WeightBlend;
    };
    UMaterialExpressionLandscapeLayerBlend *BlendNode = NewObject<UMaterialExpressionLandscapeLayerBlend>(
        Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(), NAME_None, RF_Transactional);
    BlendNode->MaterialExpressionEditorX = BaseX;
    BlendNode->MaterialExpressionEditorY = BaseY;
    TArray<FString> LayerNames;
    for (int32 i = 0; i < LayersArray->Num(); ++i) {
      const TSharedPtr<FJsonObject> *LayerObj;
      FString LayerName;
      FString BlendType = DefaultBlendType;
      if ((*LayersArray)[i]->TryGetObject(LayerObj)) {
        (*LayerObj)->TryGetStringField(TEXT("name"), LayerName);
        if (LayerName.IsEmpty()) { (*LayerObj)->TryGetStringField(TEXT("layerName"), LayerName); }
        FString LayerBlend;
        if ((*LayerObj)->TryGetStringField(TEXT("blendType"), LayerBlend) && !LayerBlend.IsEmpty()) { BlendType = LayerBlend; }
      } else {
        (*LayersArray)[i]->TryGetString(LayerName); // plain layer names are accepted too
      }
      if (LayerName.IsEmpty()) { continue; }
      FLayerBlendInput Input;
      Input.LayerName = FName(*LayerName);
      Input.BlendType = ParseBlendType(BlendType);
      Input.PreviewWeight = (i == 0) ? 1.0f : 0.0f;
      BlendNode->Layers.Add(Input);
      LayerNames.Add(LayerName);
    }
    if (BlendNode->Layers.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("No layer names found in 'layers' (use [{name, blendType?}] or [\"Name\"])"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
#if WITH_EDITORONLY_DATA
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(BlendNode);
#endif
    CreatedNodeIds.Add(MCP_NODE_ID(BlendNode));
    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetNumberField(TEXT("layerCount"), BlendNode->Layers.Num());
    Result->SetStringField(TEXT("blendNodeId"), MCP_NODE_ID(BlendNode));
    Result->SetStringField(TEXT("expressionClass"), TEXT("MaterialExpressionLandscapeLayerBlend"));

    TArray<TSharedPtr<FJsonValue>> NodeIdArray;
    for (const FString &NodeId : CreatedNodeIds) {
      NodeIdArray.Add(MakeShared<FJsonValueString>(NodeId));
    }
    Result->SetArrayField(TEXT("nodeIds"), NodeIdArray);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Layer blend configured with %d layers."),
                                          BlendNode->Layers.Num()),
                           Result);
    return true;
  }

  // ==========================================================================
  // 8.6 Utilities
  // ==========================================================================

  return false;
}
}
#endif
