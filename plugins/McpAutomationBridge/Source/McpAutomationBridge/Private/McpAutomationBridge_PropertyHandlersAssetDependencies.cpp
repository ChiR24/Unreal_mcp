#include "McpVersionCompatibility.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetAssetReferences(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_asset_references"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_asset_references")))
    return false;

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_references payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_references requires assetPath."),
                        TEXT("INVALID_ASSET"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
#else
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
#endif
  if (!AssetData.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TArray<FAssetIdentifier> Dependencies;
  AssetRegistry.GetDependencies(FAssetIdentifier(AssetData.PackageName), Dependencies);

  TArray<TSharedPtr<FJsonValue>> ReferencesArray;
  for (const FAssetIdentifier &Dep : Dependencies) {
    TSharedPtr<FJsonObject> RefObj = McpHandlerUtils::CreateResultObject();
    RefObj->SetStringField(TEXT("packageName"), Dep.PackageName.ToString());
    if (!Dep.ObjectName.IsNone()) {
      RefObj->SetStringField(TEXT("objectName"), Dep.ObjectName.ToString());
    }
    ReferencesArray.Add(MakeShared<FJsonValueObject>(RefObj));
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("assetPath"), AssetPath);
  ResultPayload->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
  ResultPayload->SetArrayField(TEXT("references"), ReferencesArray);
  ResultPayload->SetNumberField(TEXT("referenceCount"), ReferencesArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Asset references retrieved."), ResultPayload,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("get_asset_references requires editor build."),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleGetAssetDependencies(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  if (!Action.Equals(TEXT("get_asset_dependencies"), ESearchCase::IgnoreCase) &&
      !LowerAction.Contains(TEXT("get_asset_dependencies")))
    return false;

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_dependencies payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.TrimStartAndEnd().IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("get_asset_dependencies requires assetPath."),
                        TEXT("INVALID_ASSET"));
    return true;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
#else
  FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
#endif
  if (!AssetData.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Asset not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  TArray<FAssetIdentifier> Referencers;
  AssetRegistry.GetReferencers(FAssetIdentifier(AssetData.PackageName), Referencers);

  TArray<TSharedPtr<FJsonValue>> DependenciesArray;
  for (const FAssetIdentifier &Ref : Referencers) {
    TSharedPtr<FJsonObject> DepObj = McpHandlerUtils::CreateResultObject();
    DepObj->SetStringField(TEXT("packageName"), Ref.PackageName.ToString());
    if (!Ref.ObjectName.IsNone()) {
      DepObj->SetStringField(TEXT("objectName"), Ref.ObjectName.ToString());
    }
    DependenciesArray.Add(MakeShared<FJsonValueObject>(DepObj));
  }

  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("assetPath"), AssetPath);
  ResultPayload->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
  ResultPayload->SetArrayField(TEXT("dependencies"), DependenciesArray);
  ResultPayload->SetNumberField(TEXT("dependencyCount"), DependenciesArray.Num());

  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Asset dependencies retrieved."), ResultPayload,
                         FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("get_asset_dependencies requires editor build."),
                      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
