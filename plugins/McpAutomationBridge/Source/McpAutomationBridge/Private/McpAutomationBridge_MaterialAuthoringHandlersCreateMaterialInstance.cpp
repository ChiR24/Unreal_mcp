#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterialInstance(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material_instance")) {
    FString Name, Path, ParentMaterial;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);

    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material instance name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    if (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) ||
        ParentMaterial.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parentMaterial'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials");
    }

    // Validate path (same as create_material)
    FString ValidatedPath;
    FString PathError;
    if (!ValidateAssetCreationPath(Path, Name, ValidatedPath, PathError)) {
      Bridge->SendAutomationError(Socket, RequestId, PathError, TEXT("INVALID_PATH"));
      return true;
    }

    if (ValidatedPath.Contains(TEXT(":"))) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': absolute Windows paths are not allowed"), *ValidatedPath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    FText MountReason;
    if (!FPackageName::IsValidLongPackageName(ValidatedPath, true, &MountReason)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path '%s': %s"), *ValidatedPath, *MountReason.ToString()),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Check for existing asset collision
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialInstanceConstant with the same name."),
                                            *FullAssetPath, *ExistingClassName),
                            TEXT("ASSET_EXISTS"));
      } else {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists. Cannot overwrite with different asset type."),
                                            *FullAssetPath),
                            TEXT("ASSET_EXISTS"));
      }
      return true;
    }
    // SECURITY: Validate parentMaterial path before loading
    FString ValidatedParentPath = SanitizeProjectRelativePath(ParentMaterial);
    if (ValidatedParentPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid parentMaterial path '%s': contains traversal sequences or invalid root"), *ParentMaterial),
                          TEXT("INVALID_PATH"));
      return true;
    }
    ParentMaterial = ValidatedParentPath;

    UMaterial *Parent = LoadObject<UMaterial>(nullptr, *ParentMaterial);
    if (!Parent) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load parent material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    UMaterialInstanceConstantFactoryNew *Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    Factory->InitialParent = Parent;

    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialInstanceConstant *NewInstance = Cast<UMaterialInstanceConstant>(
        Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(),
                                  Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));
    if (!NewInstance) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material instance."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    NewInstance->PostEditChange();
    NewInstance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(NewInstance);
    }

    FAssetRegistryModule::AssetCreated(NewInstance);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewInstance);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material instance '%s' created."), *Name), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_scalar_parameter_value
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
