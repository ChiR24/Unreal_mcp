#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleCreateMaterialFunction(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("create_material_function")) {
    FString Name, ValidatedPath, Description;
    bool bParentFolderCreated = false;
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, GetJsonStringField(Payload, TEXT("name")),
                                 GetJsonStringField(Payload, TEXT("path")), TEXT("/Game/Materials/Functions"),
                                 TEXT("MaterialFunction"), Name, ValidatedPath, bParentFolderCreated)) {
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
      McpSafeAssetSave(NewFunc);
    }

    FAssetRegistryModule::AssetCreated(NewFunc);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, NewFunc);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material function '%s' created."), *Name), Result);
    return true;
  }

  return false;
}
}
