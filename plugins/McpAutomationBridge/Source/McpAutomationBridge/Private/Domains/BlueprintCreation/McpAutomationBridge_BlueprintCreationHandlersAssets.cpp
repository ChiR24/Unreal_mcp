#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpAutomationBridge_BlueprintCreationHandlersPrivate.h"


#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
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
  Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
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

  FString ParentError;
  UFactory *Factory = CreateBlueprintFactory(Context, ParentError);
  if (!Factory) {
    Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, ParentError, nullptr,
                                 TEXT("CLASS_NOT_FOUND"));
    return true;
  }
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
  TArray<FString> AppliedProperties, FailedProperties;
  ApplyBlueprintProperties(CreatedBlueprint, Context.Payload, AppliedProperties, FailedProperties);

  if (!CreatedBlueprint) {
    if (RespondIfBlueprintExists(Self, Context)) {
      return true;
    }

    const FString CreationError =
        FString::Printf(TEXT("Created asset is not a Blueprint: %s"),
                        NewObject ? *NewObject->GetPathName() : TEXT("<null>"));
    Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, CreationError, nullptr,
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
  FString Message = TEXT("Blueprint created");
  if (AppliedProperties.Num() + FailedProperties.Num() > 0) {
    ResultPayload->SetArrayField(TEXT("appliedProperties"), McpHandlerUtils::ToJsonStringArray(AppliedProperties));
    ResultPayload->SetArrayField(TEXT("failedProperties"), McpHandlerUtils::ToJsonStringArray(FailedProperties));
  }
  if (FailedProperties.Num() > 0) {
    // The asset exists either way, so the create stands; what was not set is named, not hidden.
    Message = FString::Printf(TEXT("Blueprint created; %d of its properties were not set: %s"),
                              FailedProperties.Num(), *FString::Join(FailedProperties, TEXT("; ")));
  }
  Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
                               Message, ResultPayload, FString());

  TWeakObjectPtr<UBlueprint> WeakCreatedBlueprint = CreatedBlueprint;
  if (WeakCreatedBlueprint.IsValid()) {
    UBlueprint *Blueprint = WeakCreatedBlueprint.Get();
    SaveLoadedAssetThrottled(Blueprint, true);
    ScanPathSynchronous(Blueprint->GetOutermost()->GetName());
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"),
         *Context.RequestId);
  return true;
}

}

