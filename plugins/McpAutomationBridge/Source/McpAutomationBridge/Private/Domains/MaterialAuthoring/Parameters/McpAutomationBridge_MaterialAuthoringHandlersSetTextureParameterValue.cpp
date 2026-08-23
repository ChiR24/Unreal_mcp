#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Safety/McpSafeOperationsOpenEditorGuard.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetTextureParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_texture_parameter_value")) {
    FString AssetPath, ParamName, TexturePath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) ||
        TexturePath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."),
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

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    UMaterial *BaseMaterial = Instance ? nullptr : LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Instance && !BaseMaterial) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load a material instance or material at this path."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    // SECURITY: Validate texturePath before loading
    FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
    if (ValidatedTexturePath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    TexturePath = ValidatedTexturePath;

    UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load texture."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // A base UMaterial has no override table; its texture parameters live on
    // UMaterialExpressionTextureSampleParameter. Mirrors the scalar and vector
    // setters, which already accept a base material here — without it a master
    // material's texture defaults are unreachable through this capability.
    if (BaseMaterial) {
      // The material editor edits a preview duplicate and writes it back over
      // the original on close, so a write taken now is silently reverted the
      // moment the tab closes. Refuse instead of reporting a success the caller
      // only discovers was a lie much later.
      if (McpSafeOperations::IsAssetEditorOpen(BaseMaterial)) {
        Bridge->SendAutomationError(
            Socket, RequestId,
            McpSafeOperations::OpenAssetEditorRefusal(BaseMaterial),
            TEXT("ASSET_EDITOR_OPEN"));
        return true;
      }
      UMaterialExpressionTextureSampleParameter *Param = nullptr;
      TArray<FString> AvailableParams;
      const FName ParamFName(*ParamName);
      for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMaterial)) {
        if (UMaterialExpressionTextureSampleParameter *Sampler =
                Cast<UMaterialExpressionTextureSampleParameter>(Expr)) {
          AvailableParams.Add(Sampler->ParameterName.ToString());
          if (Sampler->ParameterName == ParamFName) {
            Param = Sampler;
          }
        }
      }
      if (!Param) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Texture parameter '%s' not found on base material. Available: [%s]"),
                                            *ParamName, *FString::Join(AvailableParams, TEXT(", "))),
                            TEXT("PARAMETER_NOT_FOUND"));
        return true;
      }
      Param->Texture = Texture;
      BaseMaterial->PostEditChange();
      BaseMaterial->MarkPackageDirty();

      bool bSaveBase = true;
      Payload->TryGetBoolField(TEXT("save"), bSaveBase);
      if (bSaveBase) {
        SaveMaterialAsset(BaseMaterial);
      }

      TSharedPtr<FJsonObject> BaseResult = McpHandlerUtils::CreateResultObject();
      McpHandlerUtils::AddVerification(BaseResult, BaseMaterial);
      BaseResult->SetStringField(TEXT("parameterName"), ParamName);
      BaseResult->SetStringField(TEXT("note"), TEXT("Base material (not an instance): the TextureSampleParameter expression's Texture was updated."));
      Bridge->SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("Texture parameter '%s' default set on base material."), *ParamName),
          BaseResult);
      return true;
    }

    Instance->SetTextureParameterValueEditorOnly(FName(*ParamName), Texture);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Texture parameter '%s' set."), *ParamName), Result);
    return true;
  }

  // ==========================================================================
  // 8.5 Specialized Materials
  // ==========================================================================

  // --------------------------------------------------------------------------
  // create_landscape_material, create_decal_material, create_post_process_material
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
