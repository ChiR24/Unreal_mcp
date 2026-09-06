#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material")) {
    FString Name, Path;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);

    // Check if sanitization significantly changed the name (indicates invalid characters)
    // If the sanitized name is different and doesn't just have underscores added/removed
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      Bridge->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: reject Windows absolute paths (contain colon)
    if (ValidatedPath.Contains(TEXT(":"))) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Additional validation: verify mount point using engine API
    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FString ParentFolderPath = FPackageName::GetLongPackagePath(ValidatedPath);
    bool bParentFolderCreated = false;
    if (!AssetRegistry.PathExists(FName(*ParentFolderPath))) {
      // Auto-create the parent chain instead of failing: the path already passed sanitization and
      // IsValidLongPackageName above, and create_folder creates parents recursively — creating an
      // asset in a fresh folder should not require a separate call. The old error remains the
      // fallback for when directory creation itself fails.
      // PathExists (asset registry) can lag a folder that already exists on disk; check the
      // directory itself so parentFolderCreated stays truthful (MakeDirectory is idempotent).
      const bool bAlreadyOnDisk = UEditorAssetLibrary::DoesDirectoryExist(ParentFolderPath);
      if (!UEditorAssetLibrary::MakeDirectory(ParentFolderPath)) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *ParentFolderPath),
                            TEXT("PARENT_FOLDER_NOT_FOUND"));
        return true;
      }
      bParentFolderCreated = !bAlreadyOnDisk;
    }

    // Check for existing asset collision to prevent UE crash
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create Material with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
      return true;
    }
    // Create material using factory - use ValidatedPath, not original Path!
    UMaterialFactoryNew *Factory = NewObject<UMaterialFactoryNew>();
    UPackage *Package = CreatePackage(*ValidatedPath);
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

    // Set properties
    FString MaterialDomain;
    if (Payload->TryGetStringField(TEXT("materialDomain"), MaterialDomain)) {
      EMaterialDomain Parsed{};
      const bool bValidMaterialDomain = ParseMaterialDomain(MaterialDomain, Parsed);
      if (bValidMaterialDomain) {
        NewMaterial->MaterialDomain = Parsed;
      }
      if (!bValidMaterialDomain) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"), *MaterialDomain),
                            TEXT("INVALID_ENUM"));
        return true;
      }
    }

    FString BlendMode;
    if (Payload->TryGetStringField(TEXT("blendMode"), BlendMode)) {
      EBlendMode Parsed{};
      const bool bValidBlendMode = ParseBlendMode(BlendMode, Parsed);
      if (bValidBlendMode) {
        NewMaterial->BlendMode = Parsed;
      }
      if (!bValidBlendMode) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid blendMode '%s'. Valid values: Opaque, Masked, Translucent, Additive, Modulate, AlphaComposite, AlphaHoldout"), *BlendMode),
                            TEXT("INVALID_ENUM"));
        return true;
      }
    }

    FString ShadingModel;
    if (Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      EMaterialShadingModel Parsed{};
      const bool bValidShadingModel = ParseShadingModel(ShadingModel, Parsed);
      if (bValidShadingModel) {
        NewMaterial->SetShadingModel(Parsed);
      }
      if (!bValidShadingModel) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"), *ShadingModel),
                            TEXT("INVALID_ENUM"));
        return true;
      }
    }

    bool bTwoSided = false;
    if (Payload->TryGetBoolField(TEXT("twoSided"), bTwoSided)) {
      NewMaterial->TwoSided = bTwoSided;
    }

    NewMaterial->PostEditChange();
    NewMaterial->MarkPackageDirty();

    // Notify asset registry FIRST (required for UE 5.7+ before saving)
    FAssetRegistryModule::AssetCreated(NewMaterial);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(NewMaterial);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewMaterial);
    if (bParentFolderCreated) {
      Result->SetBoolField(TEXT("parentFolderCreated"), true);
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }

  return false;
}
}
#endif
