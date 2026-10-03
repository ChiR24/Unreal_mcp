#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material")) {
    // materialDomain / blendMode / shadingModel, as the set_* actions parse them. Checked before
    // anything is made: a refused create left its material behind, so the retry hit "already exists".
    FString EnumError;
    if (!ApplyMaterialEnumFields(nullptr, Payload, EnumError)) {
      Bridge->SendAutomationError(Socket, RequestId, EnumError, TEXT("INVALID_ENUM"));
      return true;
    }
    FString Name, ValidatedPath;
    bool bParentFolderCreated = false;
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, GetJsonStringField(Payload, TEXT("name")),
                                 GetJsonStringField(Payload, TEXT("path")), TEXT("/Game/Materials"), TEXT("Material"),
                                 Name, ValidatedPath, bParentFolderCreated)) {
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

    ApplyMaterialEnumFields(NewMaterial, Payload, EnumError);

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
      McpSafeAssetSave(NewMaterial);
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
