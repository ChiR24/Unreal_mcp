#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleConfigureLayerBlend(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("configure_layer_blend")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();
    const TArray<TSharedPtr<FJsonValue>> *LayersArray;
    if (!Payload->TryGetArrayField(TEXT("layers"), LayersArray) ||
        LayersArray->Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing or empty 'layers' array."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // A landscape layer blend is one LandscapeLayerBlend expression whose
    // FLayerBlendInput entries name the layers; scalar weight parameters are not a layer blend.
    FString DefaultBlendType;
    Payload->TryGetStringField(TEXT("blendType"), DefaultBlendType);
    auto ParseBlendType = [](const FString& Text) {
      if (Text.Contains(TEXT("Height"), ESearchCase::IgnoreCase)) { return LB_HeightBlend; }
      if (Text.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase)) { return LB_AlphaBlend; }
      return LB_WeightBlend;
    };
    // A type naming none of the three became a weight blend without a word.
    auto IsKnownBlendType = [](const FString& Text) {
      return Text.IsEmpty() || Text.Contains(TEXT("Weight"), ESearchCase::IgnoreCase) ||
             Text.Contains(TEXT("Height"), ESearchCase::IgnoreCase) || Text.Contains(TEXT("Alpha"), ESearchCase::IgnoreCase);
    };
    // The layers are read before the node is made: a refusal after NewObject left an orphan expression under the host.
    TArray<FLayerBlendInput> Inputs;
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
        McpHandlerUtils::TryGetJsonValueString((*LayersArray)[i], LayerName); // plain layer names are accepted too
      }
      if (LayerName.IsEmpty()) { continue; }
      if (!IsKnownBlendType(BlendType)) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown blendType '%s' for layer '%s'; use LB_WeightBlend, LB_HeightBlend or LB_AlphaBlend (Weight, Height, Alpha)."), *BlendType, *LayerName),
            TEXT("INVALID_ARGUMENT"));
        return true;
      }
      FLayerBlendInput Input;
      Input.LayerName = FName(*LayerName);
      Input.BlendType = ParseBlendType(BlendType);
      Input.PreviewWeight = (i == 0) ? 1.0f : 0.0f;
      Inputs.Add(Input);
    }
    if (Inputs.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("No layer names found in 'layers' (use [{name, blendType?}] or [\"Name\"])"), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UMaterialExpressionLandscapeLayerBlend *BlendNode = NewObject<UMaterialExpressionLandscapeLayerBlend>(
        HostOuter, UMaterialExpressionLandscapeLayerBlend::StaticClass(), NAME_None, RF_Transactional);
    BlendNode->Layers = Inputs;
    // Placed like every adder: added to the host, recompiled, and answered with nodeId and the placement fields.
    TSharedPtr<FJsonObject> Result = PlaceMaterialExpression(RequestId, Material, Function, BlendNode, X, Y);
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    Result->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(HostOuter));
    Result->SetNumberField(TEXT("layerCount"), BlendNode->Layers.Num());

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Layer blend configured with %d layers."),
                                          BlendNode->Layers.Num()),
                           Result);
    return true;
  }

  return false;
}
}
