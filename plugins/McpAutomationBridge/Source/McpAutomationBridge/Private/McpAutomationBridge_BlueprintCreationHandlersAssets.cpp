#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BlueprintCreationHandlersPrivate.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpBlueprintCreationHandlers {
namespace {

FString NormalizeBlueprintPath(UBlueprint *Blueprint,
                               const FString &LoadedPath) {
  FString NormalizedPath = LoadedPath.TrimStartAndEnd().IsEmpty()
                               ? Blueprint->GetPathName()
                               : LoadedPath;
  if (NormalizedPath.Contains(TEXT("."))) {
    NormalizedPath = NormalizedPath.Left(NormalizedPath.Find(TEXT(".")));
  }
  return NormalizedPath;
}

bool RespondIfBlueprintExists(UMcpAutomationBridgeSubsystem *Self,
                              const FRequestContext &Context) {
  FString NormalizedPath;
  FString LoadError;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(Context.CreateKey, NormalizedPath, LoadError);
  if (!Blueprint) {
    return false;
  }

  NormalizedPath = NormalizeBlueprintPath(Blueprint, NormalizedPath);
  const TSharedPtr<FJsonObject> ResultPayload =
      BuildBlueprintResult(Blueprint, NormalizedPath);
  CompleteInflightRequest(Self, Context, true,
                          TEXT("Blueprint already exists"), ResultPayload,
                          FString());
  return true;
}

}

bool ExecuteBlueprintCreation(UMcpAutomationBridgeSubsystem *Self,
                              const FRequestContext &Context) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate: Starting blueprint creation "
              "(WITH_EDITOR=1)"));

  if (RespondIfBlueprintExists(Self, Context)) {
    return true;
  }

  UFactory *Factory = CreateBlueprintFactory(Context);
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  UObject *NewObject = AssetToolsModule.Get().CreateAsset(
      Context.Name, Context.SavePath, UBlueprint::StaticClass(), Factory);
  if (NewObject) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("CreateAsset returned object: name=%s path=%s class=%s"),
           *NewObject->GetName(), *NewObject->GetPathName(),
           *NewObject->GetClass()->GetName());
  }

  UBlueprint *CreatedBlueprint = Cast<UBlueprint>(NewObject);
  ApplyBlueprintProperties(CreatedBlueprint, Context.Payload);

  if (!CreatedBlueprint) {
    if (RespondIfBlueprintExists(Self, Context)) {
      return true;
    }

    const FString CreationError =
        FString::Printf(TEXT("Created asset is not a Blueprint: %s"),
                        NewObject ? *NewObject->GetPathName() : TEXT("<null>"));
    CompleteInflightRequest(Self, Context, false, CreationError, nullptr,
                            TEXT("CREATE_FAILED"));
    return true;
  }

  const FString NormalizedPath =
      NormalizeBlueprintPath(CreatedBlueprint, CreatedBlueprint->GetPathName());
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  AssetRegistryModule.AssetCreated(CreatedBlueprint);

  const TSharedPtr<FJsonObject> ResultPayload =
      BuildBlueprintResult(CreatedBlueprint, NormalizedPath);
  const bool bCoalesced =
      CompleteInflightRequest(Self, Context, true, TEXT("Blueprint created"),
                              ResultPayload, FString());
  if (bCoalesced) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("blueprint_create RequestId=%s completed (coalesced)."),
           *Context.RequestId);
  }

  TWeakObjectPtr<UBlueprint> WeakCreatedBlueprint = CreatedBlueprint;
  if (WeakCreatedBlueprint.IsValid()) {
    UBlueprint *Blueprint = WeakCreatedBlueprint.Get();
    SaveLoadedAssetThrottled(Blueprint, -1.0, true);
    ScanPathSynchronous(Blueprint->GetOutermost()->GetName());
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"),
         *Context.RequestId);
  return true;
}

}

#endif
