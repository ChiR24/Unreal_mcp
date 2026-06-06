#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/Factory.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

namespace McpEditorFunctionHandlers {

bool HandleAssetCreationFunction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN != TEXT("CREATE_ASSET"))
    return false;

  // Check if we have a nested "params" object, which is standard for
  // ExecuteEditorFunction
  const TSharedPtr<FJsonObject> *ParamsObj;
  const TSharedPtr<FJsonObject> SourceObj =
      (Payload->TryGetObjectField(TEXT("params"), ParamsObj)) ? *ParamsObj
                                                              : Payload;

  FString AssetName;
  SourceObj->TryGetStringField(TEXT("asset_name"), AssetName);
  FString PackagePath;
  SourceObj->TryGetStringField(TEXT("package_path"), PackagePath);
  FString AssetClass;
  SourceObj->TryGetStringField(TEXT("asset_class"), AssetClass);
  FString FactoryClass;
  SourceObj->TryGetStringField(TEXT("factory_class"), FactoryClass);

  if (AssetName.IsEmpty() || PackagePath.IsEmpty() ||
      FactoryClass.IsEmpty()) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("asset_name, package_path, and factory_class required"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!GEditor) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Editor not available"), nullptr,
                                  TEXT("EDITOR_NOT_AVAILABLE"));
    return true;
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  // Resolve factory
  UClass *FactoryUClass = ResolveClassByName(FactoryClass);
  if (!FactoryUClass) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    // Try finding by short name or full path
    FactoryUClass = UClass::TryFindTypeSlow<UClass>(FactoryClass);
#else
    FactoryUClass = ResolveClassByName(FactoryClass);
#endif
  }

  // Quick factory lookup by short name if full resolution failed
  if (!FactoryUClass) {
    for (TObjectIterator<UClass> It; It; ++It) {
      if (It->GetName().Equals(FactoryClass) ||
          It->GetName().Equals(FactoryClass + TEXT("Factory"))) {
        if (It->IsChildOf(UFactory::StaticClass())) {
          FactoryUClass = *It;
          break;
        }
      }
    }
  }

  if (!FactoryUClass) {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Factory class '%s' not found"), *FactoryClass),
        nullptr, TEXT("FACTORY_NOT_FOUND"));
    return true;
  }

  UFactory *Factory =
      NewObject<UFactory>(GetTransientPackage(), FactoryUClass);
  if (!Factory) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("Failed to instantiate factory"),
                                  nullptr, TEXT("FACTORY_CREATION_FAILED"));
    return true;
  }

  // Attempt creation
  UObject *NewAsset =
      AssetTools.CreateAsset(AssetName, PackagePath, nullptr, Factory);
  if (NewAsset) {
    // Use McpSafeAssetSave instead of modal PromptForCheckoutAndSave to avoid D3D12 crashes
    McpSafeAssetSave(NewAsset);

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetStringField(TEXT("name"), NewAsset->GetName());
    Out->SetStringField(TEXT("path"), NewAsset->GetPathName());
    Out->SetBoolField(TEXT("success"), true);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Asset created"), Out, FString());
  } else {
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("Failed to create asset via AssetTools"), nullptr,
        TEXT("ASSET_CREATION_FAILED"));
  }
  return true;
}

}
#endif
