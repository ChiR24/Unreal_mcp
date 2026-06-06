#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleCreateSpecializedMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_landscape_material") ||
      SubAction == TEXT("create_decal_material") ||
      SubAction == TEXT("create_post_process_material")) {
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Name validation - sanitize and check for invalid characters
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid asset name '%s'. Names can only contain alphanumeric characters and underscores."), *OriginalName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    // Path validation - check for traversal and normalize
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      Bridge->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;

    // Check for existing asset collision (different class)
    FString FullAssetPath = Path + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Asset already exists at path: %s"), *FullAssetPath),
                          TEXT("ASSET_EXISTS"));
      return true;
    }

    // Create material using factory
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    UPackage *Package = CreatePackage(*Path);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterial *NewMaterial = Cast<UMaterial>(
        Factory->FactoryCreateNew(UMaterial::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewMaterial) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create material."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    // Set domain based on type
    if (SubAction == TEXT("create_landscape_material")) {
      // Landscape materials use Surface domain but typically have special setup
      NewMaterial->MaterialDomain = EMaterialDomain::MD_Surface;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    } else if (SubAction == TEXT("create_decal_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
    } else if (SubAction == TEXT("create_post_process_material")) {
      NewMaterial->MaterialDomain = EMaterialDomain::MD_PostProcess;
      NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(NewMaterial);
    }

    FAssetRegistryModule::AssetCreated(NewMaterial);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewMaterial);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_landscape_layer, configure_layer_blend
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
