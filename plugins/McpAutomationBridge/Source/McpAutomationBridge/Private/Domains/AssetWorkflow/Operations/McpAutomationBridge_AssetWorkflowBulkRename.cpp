// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkSelection.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
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

  // renames: [{sourcePath, newName}], unrelated names under one consent. Each rename used to need
  // its own describe for a fresh consent grant; a pattern could not express them.
  TArray<FAssetRenameData> RenameData;
  TArray<FString> MissingAssets;
  const TArray<TSharedPtr<FJsonValue>> *Renames = nullptr;
  if (Payload->TryGetArrayField(TEXT("renames"), Renames) && Renames->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Entry : *Renames) {
      const TSharedPtr<FJsonObject> *Item = nullptr;
      const FString Source = Entry.IsValid() && Entry->TryGetObject(Item) ? GetJsonStringField(*Item, TEXT("sourcePath")) : FString();
      const FString NewName = Item ? GetJsonStringField(*Item, TEXT("newName")).TrimStartAndEnd() : FString();
      const FString Resolved = Source.IsEmpty() ? FString() : ResolveAssetPath(Source);
      const FString Safe = SanitizeProjectRelativePath(Resolved.IsEmpty() ? Source : Resolved);
      UObject *Asset = Safe.IsEmpty() || NewName.IsEmpty() || !UEditorAssetLibrary::DoesAssetExist(Safe) ? nullptr : UEditorAssetLibrary::LoadAsset(Safe);
      const FString Folder = Asset ? FPackageName::GetLongPackagePath(Asset->GetOutermost()->GetName()) : FString();
      if (!Asset || UEditorAssetLibrary::DoesAssetExist(Folder / NewName)) {
        MissingAssets.Add(Asset ? FString::Printf(TEXT("%s (%s is taken)"), *Source, *NewName)
                                : Source.IsEmpty() ? FString(TEXT("(entry without sourcePath)")) : Source);
        continue;
      }
      RenameData.Emplace(Asset, Folder, NewName);
    }
  }
  TArray<FString> AssetPaths;
  if (RenameData.Num() == 0 && MissingAssets.Num() == 0 &&
      !McpCollectBulkAssetPaths(*this, RequestId, RequestingSocket, Payload, AssetPaths)) {
    return true;
  }
  if (RenameData.Num() == 0 && MissingAssets.Num() == 0 && AssetPaths.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("renamed"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
    return true;
  }

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

  // The old paths are read before the rename: afterwards Asset->GetPathName() is the NEW path.
  TArray<TSharedPtr<FJsonValue>> RenamedAssets;
  for (const FAssetRenameData &Data : RenameData) {
    TSharedPtr<FJsonObject> AssetInfo = McpHandlerUtils::CreateResultObject();
    AssetInfo->SetStringField(TEXT("oldPath"), Data.Asset->GetPathName());
    AssetInfo->SetStringField(TEXT("newName"), Data.NewName);
    RenamedAssets.Add(MakeShared<FJsonValueObject>(AssetInfo));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  FString Failure;
  const bool bSuccess = McpAssetRename::RenameWithSettingsFollow(RenameData, Result, Failure);
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("renamed"), bSuccess ? RenameData.Num() : 0);
  Result->SetArrayField(bSuccess ? TEXT("assets") : TEXT("notRenamed"), RenamedAssets);
  // An entry that matched nothing (or wanted a taken name) is named, not dropped.
  if (MissingAssets.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> Skipped;
    for (const FString &Missing : MissingAssets) {
      Skipped.Add(MakeShared<FJsonValueString>(Missing));
    }
    Result->SetArrayField(TEXT("skipped"), Skipped);
  }

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? FString::Printf(TEXT("Renamed %d assets"), RenameData.Num()) : Failure,
      Result, bSuccess ? FString() : TEXT("BULK_RENAME_FAILED"));
  return true;
}
