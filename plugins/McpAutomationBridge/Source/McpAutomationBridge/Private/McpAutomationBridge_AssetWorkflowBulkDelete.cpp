// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBulkDeleteAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_delete_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_delete"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
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

  // Check for assetPaths array first
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    // Check for folderPath - if provided, list all assets in that folder
    FString FolderPath;
    FString Pattern;
    Payload->TryGetStringField(TEXT("folderPath"), FolderPath);
    Payload->TryGetStringField(TEXT("path"), FolderPath);  // alias
    Payload->TryGetStringField(TEXT("pattern"), Pattern);

    if (!FolderPath.IsEmpty()) {
      // Normalize path
      FString NormalizedPath = FolderPath;
      if (NormalizedPath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) {
        NormalizedPath = FString::Printf(TEXT("/Game%s"), *NormalizedPath.RightChop(8));
      }

      NormalizedPath = SanitizeProjectRelativePath(NormalizedPath);
      if (NormalizedPath.IsEmpty()) {
        SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Invalid folderPath: %s"), *FolderPath),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }

      // Get all assets in the folder
      FAssetRegistryModule &AssetRegistryModule =
          FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
      IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

      FARFilter Filter;
      Filter.PackagePaths.Add(FName(*NormalizedPath));
      Filter.bRecursivePaths = true;

      // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
      // Asset listing uses cached AssetRegistry data exclusively.
      // LIMITATION: Assets not yet indexed by the editor's background scanner
      // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
      TArray<FAssetData> AssetDataList;
      AssetRegistry.GetAssets(Filter, AssetDataList);

      for (const FAssetData &AssetData : AssetDataList) {
        FString AssetPath = AssetData.ToSoftObjectPath().ToString();
        // If pattern is specified, filter by it
        if (!Pattern.IsEmpty()) {
          FString AssetName = AssetData.AssetName.ToString();
          if (!AssetName.Contains(Pattern)) {
            continue;
          }
        }
        AssetPaths.Add(AssetPath);
      }

      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("deleted"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found matching criteria"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
        return true;
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Either assetPaths array or folderPath is required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
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

    TArray<FAssetData> RedirectorAssets;
    AssetRegistry.GetAssets(Filter, RedirectorAssets);

    if (RedirectorAssets.Num() > 0) {
      TArray<UObjectRedirector *> Redirectors;
      for (const FAssetData &Asset : RedirectorAssets) {
        if (UObjectRedirector *Redirector =
                Cast<UObjectRedirector>(Asset.GetAsset())) {
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

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Path : ValidPaths) {
    DeletedArray.Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num());

  SendAutomationResponse(
      RequestingSocket, RequestId, DeletedCount > 0,
      FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount,
                      ObjectsToDelete.Num()),
      Result, DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_delete requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 6. GENERATE THUMBNAIL
// ============================================================================
