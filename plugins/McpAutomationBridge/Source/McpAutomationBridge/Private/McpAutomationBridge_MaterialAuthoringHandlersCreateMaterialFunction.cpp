#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material_function")) {
    FString Name, Path, Description;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'name'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate and sanitize the asset name (same as create_material)
    FString OriginalName = Name;
    FString SanitizedName = SanitizeAssetName(Name);

    // Check if sanitization significantly changed the name (indicates invalid characters)
    FString NormalizedOriginal = OriginalName.Replace(TEXT("_"), TEXT(""));
    FString NormalizedSanitized = SanitizedName.Replace(TEXT("_"), TEXT(""));
    if (NormalizedSanitized != NormalizedOriginal) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid material function name '%s': contains characters that cannot be used in asset names. Valid name would be: '%s'"),
                                          *OriginalName, *SanitizedName),
                          TEXT("INVALID_NAME"));
      return true;
    }
    Name = SanitizedName;

    Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty()) {
      Path = TEXT("/Game/Materials/Functions");
    }

    // Validate path doesn't contain traversal sequences (same as create_material)
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

    // Check for existing asset collision to prevent UE crash
    // Creating a MaterialFunction over an existing Material causes fatal error
    FString FullAssetPath = ValidatedPath + TEXT(".") + Name;
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath)) {
      // Get the existing asset's class to provide helpful error
      UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
      if (ExistingAsset) {
        UClass* ExistingClass = ExistingAsset->GetClass();
        FString ExistingClassName = ExistingClass ? ExistingClass->GetName() : TEXT("Unknown");
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Asset '%s' already exists as %s. Cannot create MaterialFunction with the same name."),
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

    Payload->TryGetStringField(TEXT("description"), Description);

    bool bExposeToLibrary = true;
    Payload->TryGetBoolField(TEXT("exposeToLibrary"), bExposeToLibrary);

    // Create function using factory - use ValidatedPath, not original Path!
    UMaterialFunctionFactoryNew *Factory =
        NewObject<UMaterialFunctionFactoryNew>();
    UPackage *Package = CreatePackage(*ValidatedPath);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."),
                          TEXT("PACKAGE_ERROR"));
      return true;
    }

    UMaterialFunction *NewFunc = Cast<UMaterialFunction>(
        Factory->FactoryCreateNew(UMaterialFunction::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewFunc) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create material function."),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    if (!Description.IsEmpty()) {
      NewFunc->Description = Description;
    }
    NewFunc->bExposeToLibrary = bExposeToLibrary;

    NewFunc->PostEditChange();
    NewFunc->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialFunctionAsset(NewFunc);
    }

    FAssetRegistryModule::AssetCreated(NewFunc);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewFunc);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material function '%s' created."), *Name), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_function_input / add_function_output
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
