#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

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
    if (!Instance) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
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
