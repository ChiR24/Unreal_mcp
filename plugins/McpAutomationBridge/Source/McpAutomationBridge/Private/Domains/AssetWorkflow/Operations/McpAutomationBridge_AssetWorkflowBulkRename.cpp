// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkSelection.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "IAssetTools.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"

bool UMcpAutomationBridgeSubsystem::HandleBulkRenameAssets(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("bulk_rename_assets"), ESearchCase::IgnoreCase) &&
      !Lower.Equals(TEXT("bulk_rename"), ESearchCase::IgnoreCase)) {
    return false;
  }
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
  // pattern / replacement are the declared fallbacks when searchText / replaceText are absent.
  SearchText = GetJsonStringField(Payload, TEXT("searchText"), GetJsonStringField(Payload, TEXT("pattern")));
  ReplaceText = GetJsonStringField(Payload, TEXT("replaceText"), GetJsonStringField(Payload, TEXT("replacement")));

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  TArray<FString> AssetPaths;
  if (!McpCollectBulkAssetPaths(*this, RequestId, RequestingSocket, Payload, AssetPaths)) {
    return true;
  }
  if (AssetPaths.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("renamed"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
    return true;
  }

  TArray<FAssetRenameData> RenameData;
  TArray<FString> MissingAssets;

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
      MissingAssets.Add(AssetPath);
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

  if (RenameData.Num() == 0 && MissingAssets.Num() > 0) {
    // Nothing renamed because the inputs do not exist (dogfood #198): that is an error, not a no-op.
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Assets not found: %s"), *FString::Join(MissingAssets, TEXT(", "))),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
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
}
