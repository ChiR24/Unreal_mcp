// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "AutomatedAssetImportData.h"
#include "EditorAssetLibrary.h"
#include "IAssetTools.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleImportAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);

  if (DestinationPath.IsEmpty() || SourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ResolvedSourcePath;
  FString SourcePathError;
  if (!McpResolveProjectFilePath(
          SourcePath, ResolvedSourcePath, SourcePathError)) {
    SendAutomationResponse(
        Socket, RequestId, false, SourcePathError, nullptr,
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Verify source file exists
  if (!FPaths::FileExists(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source file not found: %s"), *SourcePath),
        nullptr, TEXT("SOURCE_NOT_FOUND"));
    return true;
  }

  // Sanitize destination path
  FString SafeDestPath = SanitizeProjectRelativePath(DestinationPath);
  if (SafeDestPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destination path"), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }

  FString DestPath = FPaths::GetPath(SafeDestPath);
  FString DestName = FPaths::GetBaseFilename(SafeDestPath);

  // If destination is just a folder, use that
  if (FPaths::GetExtension(SafeDestPath).IsEmpty()) {
    DestPath = SafeDestPath;
    DestName = FPaths::GetBaseFilename(SourcePath);
  }

  // Sanitize DestName: UE asset names cannot contain spaces or dots
  DestName.ReplaceInline(TEXT(" "), TEXT("_"));
  DestName.ReplaceInline(TEXT("."), TEXT("_"));

  // Defer the import to the next tick to avoid TaskGraph recursion issues with
  // UE 5.7+ Interchange Framework. See issue #137.
  // We use SetTimerForNextTick to ensure we're completely outside of any
  // TaskGraph callback chain before invoking the import.
  if (GEditor) {
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    GEditor->GetTimerManager()->SetTimerForNextTick(
        [WeakThis, RequestId, ResolvedSourcePath, DestPath, DestName, Socket]() {
          UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
          if (!StrongThis) {
            return;
          }

          IAssetTools &AssetTools =
              FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
                  .Get();

          TArray<FString> Files;
          Files.Add(ResolvedSourcePath);

          UAutomatedAssetImportData *ImportData =
              NewObject<UAutomatedAssetImportData>();
          ImportData->bReplaceExisting = true;
          ImportData->DestinationPath = DestPath;
          ImportData->Filenames = Files;

          TArray<UObject *> ImportedAssets =
              AssetTools.ImportAssetsAutomated(ImportData);

          // Find the first valid (non-null) asset in the array.
          // ImportAssetsAutomated can return arrays with nullptr entries.
          UObject *Asset = nullptr;
          for (UObject *ImportedObj : ImportedAssets) {
            if (ImportedObj) {
              Asset = ImportedObj;
              break;
            }
          }

          if (Asset) {
            // Compute the final asset path. If we rename, use the destination
            // path/name since RenameAssets may invalidate the Asset pointer.
            FString FinalAssetPath;
            bool bRenameSucceeded = true;

            // Rename if needed
            if (Asset->GetName() != DestName) {
              FAssetRenameData RenameData(Asset, DestPath, DestName);
              bRenameSucceeded = AssetTools.RenameAssets({RenameData});
              // After rename, compute path from destination (Asset pointer may
              // be stale)
              FinalAssetPath = DestPath / DestName + TEXT(".") + DestName;
            } else {
              // No rename needed, safe to use the asset's current path
              FinalAssetPath = Asset->GetPathName();
            }

            TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("assetPath"), FinalAssetPath);
            if (!bRenameSucceeded) {
              Resp->SetBoolField(TEXT("renameWarning"), true);
            }
            // Add verification data
            UObject *ImportedAsset = UEditorAssetLibrary::LoadAsset(FinalAssetPath);
            if (ImportedAsset) {
              McpHandlerUtils::AddVerification(Resp, ImportedAsset);
            }
            StrongThis->SendAutomationResponse(
                Socket, RequestId, true,
                bRenameSucceeded ? TEXT("Asset imported")
                                 : TEXT("Asset imported but rename failed"),
                Resp, FString());
          } else {
            StrongThis->SendAutomationResponse(
                Socket, RequestId, false,
                FString::Printf(TEXT("Failed to import asset from '%s'"),
                                 *ResolvedSourcePath),
                nullptr, TEXT("IMPORT_FAILED"));
          }
        });
  } else {
    // Fallback: GEditor not available (shouldn't happen in editor context)
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Editor not available for deferred import"),
                           nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
  }

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles metadata setting requests for assets.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and 'metadata' object.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

bool UMcpAutomationBridgeSubsystem::HandleDuplicateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // Auto-resolve simple name for destination
  if (!DestinationPath.IsEmpty() &&
      FPaths::GetPath(DestinationPath).IsEmpty()) {
    FString ParentDir = FPaths::GetPath(SourcePath);
    if (ParentDir.IsEmpty() || ParentDir == TEXT("/"))
      ParentDir = TEXT("/Game");

    DestinationPath = ParentDir / DestinationPath;
  }

  SourcePath = SanitizeProjectRelativePath(SourcePath);
  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath or destinationPath"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // If the source path is a directory, perform a deep duplication of all
  // assets under that folder into the destination folder, preserving
  // relative structure. This powers the "Deep Duplication - Duplicate
  // Folder" scenario in tests.
  if (UEditorAssetLibrary::DoesDirectoryExist(SourcePath)) {
    // Ensure the destination root exists
    UEditorAssetLibrary::MakeDirectory(DestinationPath);

    FAssetRegistryModule &AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*SourcePath));
    Filter.bRecursivePaths = true;

    // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
    // Asset listing uses cached AssetRegistry data exclusively.
    // LIMITATION: Assets not yet indexed by the editor's background scanner
    // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);

    int32 DuplicatedCount = 0;
    for (const FAssetData &Asset : Assets) {
      // PackageName is the long package path (e.g.,
      // /Game/Tests/DeepCopy/Source/M_Source)
      const FString SourceAssetPath = Asset.PackageName.ToString();

      FString RelativePath;
      if (SourceAssetPath.StartsWith(SourcePath)) {
        RelativePath = SourceAssetPath.RightChop(SourcePath.Len());
      } else {
        // Should not happen for the filtered set, but skip if it does.
        continue;
      }

      const FString TargetAssetPath =
          DestinationPath + RelativePath; // preserves any subfolders
      const FString TargetFolderPath = FPaths::GetPath(TargetAssetPath);
      if (!TargetFolderPath.IsEmpty()) {
        UEditorAssetLibrary::MakeDirectory(TargetFolderPath);
      }

      if (UEditorAssetLibrary::DuplicateAsset(SourceAssetPath,
                                              TargetAssetPath)) {
        ++DuplicatedCount;
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    const bool bSuccess = DuplicatedCount > 0;
    Resp->SetBoolField(TEXT("success"), bSuccess);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetNumberField(TEXT("duplicatedCount"), DuplicatedCount);

    if (bSuccess) {
      SendAutomationResponse(Socket, RequestId, true, TEXT("Folder duplicated"),
                             Resp, FString());
    } else {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("No assets duplicated"), Resp,
                             TEXT("DUPLICATE_FAILED"));
    }
    return true;
  }

  // Fallback: single-asset duplication
  if (!UEditorAssetLibrary::DoesAssetExist(SourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  if (UEditorAssetLibrary::DoesAssetExist(DestinationPath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Destination asset already exists: %s"),
                        *DestinationPath),
        nullptr, TEXT("DESTINATION_EXISTS"));
    return true;
  }

  if (UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);
    // Add verification data
    UObject *NewAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (NewAsset) {
      McpHandlerUtils::AddVerification(Resp, NewAsset);
    }
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset duplicated"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Duplicate failed"),
                           nullptr, TEXT("DUPLICATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset renaming (and moving) requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
