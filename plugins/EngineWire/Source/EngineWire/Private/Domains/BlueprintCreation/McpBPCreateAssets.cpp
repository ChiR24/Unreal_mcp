#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpBPCreatePrivate.h"


#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "EngineWireSubsystem.h"

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

TSharedPtr<FJsonObject> BuildBlueprintResult(UBlueprint *Blueprint,
                                             const FString &NormalizedPath) {
  TSharedPtr<FJsonObject> ResultPayload =
      McpHandlerUtils::CreateResultObject();
  // `blueprintPath` is the name the capability output schema declares and marks
  // required. McpProjectCanonicalOutput copies only fields the schema names, so
  // emitting solely `path`/`assetPath` projected to an empty payload and turned
  // every SUCCESSFUL create into OUTPUT_SCHEMA_VIOLATION — the caller was told
  // the Blueprint failed while it was already saved on disk. `assetPath`, the
  // object path, reaches the caller in details.
  ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedPath);
  ResultPayload->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
  ResultPayload->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
  McpHandlerUtils::AddVerification(ResultPayload, Blueprint);
  return ResultPayload;
}

bool RespondIfBlueprintExists(UEngineWireSubsystem *Self,
                              const FRequestContext &Context) {
  FString NormalizedPath;
  FString LoadError;
  UBlueprint *Blueprint =
      LoadBlueprintAsset(Context.CreateKey, NormalizedPath, LoadError);
  if (!Blueprint) {
    return false;
  }

  NormalizedPath = NormalizeBlueprintPath(Blueprint, NormalizedPath);
  // Whatever parent it had, the Blueprint already there answered success: asked for a Character, a caller got the
  // Actor Blueprint at that path and built on it.
  FString Unused;
  const UBlueprintFactory *Wanted = Context.ParentClassSpec.IsEmpty() && Context.BlueprintTypeSpec.IsEmpty()
      ? nullptr : Cast<UBlueprintFactory>(CreateBlueprintFactory(Context, Unused));
  if (Wanted && Wanted->ParentClass && Blueprint->ParentClass != Wanted->ParentClass) {
    Self->SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s already exists with parent class %s, not %s; give the new Blueprint another name. Nothing "
                             "was changed."), *NormalizedPath, *GetNameSafe(Blueprint->ParentClass), *Wanted->ParentClass->GetName()),
        TEXT("ASSET_ALREADY_EXISTS"));
    return true;
  }
  const TSharedPtr<FJsonObject> ResultPayload =
      BuildBlueprintResult(Blueprint, NormalizedPath);
  ResultPayload->SetBoolField(TEXT("alreadyExists"), true);
  McpHandlerUtils::MarkNoAssetsChanged(ResultPayload);
  Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
                          TEXT("Blueprint already exists"), ResultPayload,
                          FString());
  return true;
}

}

bool ExecuteBlueprintCreation(UEngineWireSubsystem *Self,
                              const FRequestContext &Context) {
  UE_LOG(LogEngineWireSubsystem, Log,
         TEXT("HandleBlueprintCreate: Starting blueprint creation "
              "(WITH_EDITOR=1)"));

  if (RespondIfBlueprintExists(Self, Context)) {
    return true;
  }

  FString ParentError;
  UFactory *Factory = CreateBlueprintFactory(Context, ParentError);
  if (!Factory) {
    Self->SendAutomationError(Context.RequestingSocket, Context.RequestId, ParentError,
                              TEXT("CLASS_NOT_FOUND"));
    return true;
  }
  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  UObject *NewObject = AssetToolsModule.Get().CreateAsset(
      Context.Name, Context.SavePath, UBlueprint::StaticClass(), Factory);
  if (NewObject) {
    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("CreateAsset returned object: name=%s path=%s class=%s"),
           *NewObject->GetName(), *NewObject->GetPathName(),
           *NewObject->GetClass()->GetName());
  }

  UBlueprint *CreatedBlueprint = Cast<UBlueprint>(NewObject);
  // A 5.0 factory has no BlueprintType to set, so the interface type is set on the asset it made.
  if (CreatedBlueprint && Context.BlueprintTypeSpec.Equals(TEXT("Interface"), ESearchCase::IgnoreCase) &&
      Context.ParentClassSpec.IsEmpty()) {
    CreatedBlueprint->BlueprintType = BPTYPE_Interface;
  }
  TArray<FString> AppliedProperties, FailedProperties;
  ApplyBlueprintProperties(CreatedBlueprint, Context.Payload, AppliedProperties, FailedProperties);

  if (!CreatedBlueprint) {
    if (RespondIfBlueprintExists(Self, Context)) {
      return true;
    }

    const FString CreationError =
        FString::Printf(TEXT("Created asset is not a Blueprint: %s"),
                        NewObject ? *NewObject->GetPathName() : TEXT("<null>"));
    Self->SendAutomationError(Context.RequestingSocket, Context.RequestId, CreationError,
                            TEXT("CREATE_FAILED"));
    return true;
  }

  const FString NormalizedPath =
      NormalizeBlueprintPath(CreatedBlueprint, CreatedBlueprint->GetPathName());

  // Saved before the reply, which answered saved: true and then saved.
  const bool bSaved = SaveLoadedAssetThrottled(CreatedBlueprint, true);
  ScanPathSynchronous(CreatedBlueprint->GetOutermost()->GetName());
  const TSharedPtr<FJsonObject> ResultPayload =
      BuildBlueprintResult(CreatedBlueprint, NormalizedPath);
  ResultPayload->SetBoolField(TEXT("saved"), bSaved);
  FString Message = TEXT("Blueprint created");
  // Each list only when it names something: every create that set its properties carried "failedProperties": [].
  if (AppliedProperties.Num() > 0) {
    ResultPayload->SetArrayField(TEXT("appliedProperties"), McpHandlerUtils::ToJsonStringArray(AppliedProperties));
  }
  if (FailedProperties.Num() > 0) {
    ResultPayload->SetArrayField(TEXT("failedProperties"), McpHandlerUtils::ToJsonStringArray(FailedProperties));
    // The asset exists either way, so the create stands; what was not set is named, not hidden.
    Message = FString::Printf(TEXT("Blueprint created; %d of its properties were not set: %s"),
                              FailedProperties.Num(), *FString::Join(FailedProperties, TEXT("; ")));
  }
  Self->SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
                               Message, ResultPayload, FString());

  UE_LOG(LogEngineWireSubsystem, Log,
         TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"),
         *Context.RequestId);
  return true;
}

}

