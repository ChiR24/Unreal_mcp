#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterialInstance(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material_instance")) {
    FString Name, ValidatedPath, ParentMaterial;
    bool bParentFolderCreated = false;
    if (!Payload->TryGetStringField(TEXT("parentMaterial"), ParentMaterial) ||
        ParentMaterial.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parentMaterial'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    // savePath is the published spelling (reading only `path` once dropped it and created
    // the asset under the default folder while reporting success); path is the legacy one.
    FString RequestedPath = GetJsonStringField(Payload, TEXT("savePath"));
    if (RequestedPath.IsEmpty()) {
      RequestedPath = GetJsonStringField(Payload, TEXT("path"));
    }
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, GetJsonStringField(Payload, TEXT("name")),
                                 RequestedPath, TEXT("/Game/Materials"), TEXT("MaterialInstanceConstant"),
                                 Name, ValidatedPath, bParentFolderCreated)) {
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
      McpSafeAssetSave(NewInstance);
    }

    FAssetRegistryModule::AssetCreated(NewInstance);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewInstance);
    // parameters: the instance comes out already tinted, under the one consent
    // this call carried, instead of a consented set_material_parameter per value.
    const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
    if (Payload->TryGetArrayField(TEXT("parameters"), Entries) && Entries->Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> Results;
      TArray<FString> Failed;
      ApplyMaterialParameterList(Bridge, RequestId, NewInstance->GetOutermost()->GetName(), *Entries, Socket, Results, Failed);
      Result->SetArrayField(TEXT("parameters"), Results);
      if (Failed.Num() > 0) {
        Bridge->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material instance '%s' created, but %d of %d parameters did not apply: %s"),
                            *Name, Failed.Num(), Results.Num(), *FString::Join(Failed, TEXT("; "))),
            Result, TEXT("PARAMETER_BATCH_INCOMPLETE"));
        return true;
      }
    }
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material instance '%s' created."), *Name), Result);
    return true;
  }

  return false;
}
}
