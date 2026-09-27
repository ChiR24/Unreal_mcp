// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkSelection.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_delete payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  bool bShowConfirmation = false;
  Payload->TryGetBoolField(TEXT("showConfirmation"), bShowConfirmation);

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;
  if (!McpCollectBulkAssetPaths(*this, RequestId, RequestingSocket, Payload, AssetPaths)) {
    return true;
  }
  if (AssetPaths.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("deleted"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
    return true;
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (!SafeAssetPath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(SafeAssetPath);
      }
    }
  }

  if (ObjectsToDelete.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), TEXT("No valid assets found"));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("No valid assets"), Result,
                           TEXT("NO_VALID_ASSETS"));
    return true;
  }

  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, bShowConfirmation);

  if (bFixupRedirectors && DeletedCount > 0) {
    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"),
                                             TEXT("ObjectRedirector")));
#else
    Filter.ClassNames.Add(FName(TEXT("ObjectRedirector")));
#endif

    // Scoped to the folders this call actually deleted from. Without any
    // PackagePaths the filter matched EVERY redirector in the project, so
    // deleting one folder dragged unrelated content through a referencer
    // fixup -- slow, and far wider a mutation than the caller asked for.
    for (const FString &Path : ValidPaths) {
      const FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      if (!PackageName.IsEmpty()) {
        Filter.PackagePaths.AddUnique(
            FName(*FPackageName::GetLongPackagePath(PackageName)));
      }
    }
    Filter.bRecursivePaths = true;

    // Nothing to scope to means nothing to fix up. Returning here instead
    // would skip the response below and leave the caller waiting.
    TArray<FAssetData> RedirectorAssets;
    if (Filter.PackagePaths.Num() > 0) {
      AssetRegistry.GetAssets(Filter, RedirectorAssets);
    }

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        UObjectRedirector *Redirector =
            Cast<UObjectRedirector>(Asset.GetAsset());
        // A redirector whose destination is gone is exactly what this pass
        // creates, and handing one to FixupReferencers asserts inside
        // AssetTools on an unset TOptional -- which takes the whole editor
        // down rather than failing the call. Skipping them is the difference
        // between a tidy-up and a crash.
        if (Redirector != nullptr && Redirector->DestinationObject != nullptr) {
          Redirectors.Add(Redirector);
        }
      }

      if (Redirectors.Num() > 0) {
        IAssetTools &AssetTools =
            FModuleManager::LoadModuleChecked<FAssetToolsModule>(
                TEXT("AssetTools"))
                .Get();
        AssetTools.FixupReferencers(Redirectors);
      }
    }
  }

  // ObjectTools::DeleteObjects returns a count, not a list, and reporting
  // every attempted path as deleted told callers an asset was gone while it
  // was still on disk -- worse than saying nothing, because a caller that
  // trusts it skips the retry. Ask whether each one is actually gone.
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  TArray<TSharedPtr<FJsonValue>> RemainingArray;
  for (const FString &Path : ValidPaths) {
    const bool bGone = !UEditorAssetLibrary::DoesAssetExist(Path);
    (bGone ? DeletedArray : RemainingArray)
        .Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());
  if (RemainingArray.Num() > 0) {
    Result->SetArrayField(TEXT("remaining"), RemainingArray);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
}
