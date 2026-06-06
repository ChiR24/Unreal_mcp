// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "IAssetTools.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("bulk_rename payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get rename options
  FString Prefix, Suffix, SearchText, ReplaceText;
  Payload->TryGetStringField(TEXT("prefix"), Prefix);
  Payload->TryGetStringField(TEXT("suffix"), Suffix);
  Payload->TryGetStringField(TEXT("searchText"), SearchText);
  Payload->TryGetStringField(TEXT("replaceText"), ReplaceText);

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

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
    if (Payload->TryGetStringField(TEXT("folderPath"), FolderPath) && !FolderPath.IsEmpty()) {
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
        AssetPaths.Add(AssetData.ToSoftObjectPath().ToString());
      }

      if (AssetPaths.Num() == 0) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetNumberField(TEXT("renamed"), 0);
        Result->SetStringField(TEXT("message"), TEXT("No assets found in folder"));
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

  TArray<FAssetRenameData> RenameData;

  for (const FString &InputPath : AssetPaths) {
    FString AssetPath = ResolveAssetPath(InputPath);
    if (AssetPath.IsEmpty()) {
      AssetPath = InputPath;
    }

    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty()) {
      continue;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
      continue;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
    if (!Asset) {
      continue;
    }

    FString CurrentName = Asset->GetName();
    FString NewName = CurrentName;

    if (!SearchText.IsEmpty()) {
      NewName =
          NewName.Replace(*SearchText, *ReplaceText, ESearchCase::IgnoreCase);
    }

    if (!Prefix.IsEmpty()) {
      NewName = Prefix + NewName;
    }
    if (!Suffix.IsEmpty()) {
      NewName = NewName + Suffix;
    }

    if (NewName == CurrentName) {
      continue;
    }

    FString PackagePath =
        FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName());
    FAssetRenameData RenameEntry(Asset, PackagePath, NewName);
    RenameData.Add(RenameEntry);
  }

  if (RenameData.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("renamed"), 0);
    Result->SetStringField(TEXT("message"),
                           TEXT("No assets required renaming"));
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("No renames needed"), Result, FString());
    return true;
  }

  if (bCheckoutFiles && ISourceControlModule::Get().IsEnabled()) {
    TArray<FString> PackageNames;
    for (const FAssetRenameData &Data : RenameData) {
      PackageNames.Add(Data.Asset->GetOutermost()->GetName());
    }
    SourceControlHelpers::CheckOutFiles(PackageNames, true);
  }

  IAssetTools &AssetTools =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
          .Get();
  bool bSuccess = AssetTools.RenameAssets(RenameData);

  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = McpHandlerUtils::CreateResultObject();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), RenameData.Num());
  Result->SetArrayField(TEXT("assets"), RenamedAssets);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num())
               : TEXT("Bulk rename failed"),
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("bulk_rename requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 5. BULK DELETE ASSETS
// ============================================================================
