#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

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

    // Validate and sanitize the asset name
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

    // Validate path doesn't contain traversal sequences
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

    // Validate parent folder exists
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FString ParentFolderPath = FPackageName::GetLongPackagePath(ValidatedPath);
    if (!AssetRegistry.PathExists(FName(*ParentFolderPath))) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Parent folder does not exist: %s. Create the folder first or use an existing path."), *ParentFolderPath),
                          TEXT("PARENT_FOLDER_NOT_FOUND"));
      return true;
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
      bool bValidMaterialDomain = false;
      if (MaterialDomain == TEXT("Surface")) {
        NewMaterial->MaterialDomain = MD_Surface;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("DeferredDecal")) {
        NewMaterial->MaterialDomain = MD_DeferredDecal;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("LightFunction")) {
        NewMaterial->MaterialDomain = MD_LightFunction;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("Volume")) {
        NewMaterial->MaterialDomain = MD_Volume;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("PostProcess")) {
        NewMaterial->MaterialDomain = MD_PostProcess;
        bValidMaterialDomain = true;
      } else if (MaterialDomain == TEXT("UI")) {
        NewMaterial->MaterialDomain = MD_UI;
        bValidMaterialDomain = true;
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
      bool bValidBlendMode = false;
      if (BlendMode == TEXT("Opaque")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Opaque;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Masked")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Masked;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Translucent")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Translucent;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Additive")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Additive;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("Modulate")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_Modulate;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("AlphaComposite")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaComposite;
        bValidBlendMode = true;
      } else if (BlendMode == TEXT("AlphaHoldout")) {
        NewMaterial->BlendMode = EBlendMode::BLEND_AlphaHoldout;
        bValidBlendMode = true;
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
      bool bValidShadingModel = false;
      if (ShadingModel == TEXT("Unlit")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("DefaultLit")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Subsurface")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("SubsurfaceProfile")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("PreintegratedSkin")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("ClearCoat")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Hair")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Hair);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Cloth")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("Eye")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_Eye);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("TwoSidedFoliage")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
        bValidShadingModel = true;
      } else if (ShadingModel == TEXT("ThinTranslucent")) {
        NewMaterial->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
        bValidShadingModel = true;
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
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Material '%s' created."), *Name),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_blend_mode
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
