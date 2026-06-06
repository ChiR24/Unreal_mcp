#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetBlendMode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_blend_mode")) {
    FString AssetPath, BlendMode;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'blendMode'."),
                          TEXT("INVALID_ARGUMENT"));
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
    UMaterialFunction *TmpFunc = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc);
    if (!Material && !TmpFunc) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("set_blend_mode is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidBlendMode = false;
    if (BlendMode == TEXT("Opaque")) {
      Material->BlendMode = EBlendMode::BLEND_Opaque;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Masked")) {
      Material->BlendMode = EBlendMode::BLEND_Masked;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Translucent")) {
      Material->BlendMode = EBlendMode::BLEND_Translucent;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Additive")) {
      Material->BlendMode = EBlendMode::BLEND_Additive;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("Modulate")) {
      Material->BlendMode = EBlendMode::BLEND_Modulate;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaComposite")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaComposite;
      bValidBlendMode = true;
    } else if (BlendMode == TEXT("AlphaHoldout")) {
      Material->BlendMode = EBlendMode::BLEND_AlphaHoldout;
      bValidBlendMode = true;
    }

    if (!bValidBlendMode) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"),
                                          *BlendMode),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Blend mode set to %s."), *BlendMode), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_shading_model
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
