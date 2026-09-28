// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Misc/PackageName.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"

bool UMcpAutomationBridgeSubsystem::HandleRenameAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SourcePath;
  Payload->TryGetStringField(TEXT("sourcePath"), SourcePath);
  FString DestinationPath;
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  // rename {sourcePath, newName}: the destination is the same folder under the new name (dogfood #195).
  if (DestinationPath.IsEmpty() && !SourcePath.IsEmpty()) {
    FString NewName;
    if (Payload->TryGetStringField(TEXT("newName"), NewName) && !NewName.TrimStartAndEnd().IsEmpty()) {
      FString ObjectPath = SourcePath;
      int32 DotIndex = INDEX_NONE;
      if (ObjectPath.FindChar(TEXT('.'), DotIndex)) {
        ObjectPath.LeftInline(DotIndex);
      }
      DestinationPath = FPackageName::GetLongPackagePath(ObjectPath) / NewName.TrimStartAndEnd();
    }
  }

  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sourcePath and destinationPath (or newName) required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // A bare destination name stays beside the source.
  DestinationPath = McpHandlerUtils::ResolveSiblingAssetPath(SourcePath, DestinationPath);

  if ((SourcePath.Contains(TEXT("/")) || SourcePath.StartsWith(TEXT("/"))) &&
      SanitizeProjectRelativePath(SourcePath).IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  DestinationPath = SanitizeProjectRelativePath(DestinationPath);
  if (DestinationPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid destinationPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Resolve source path to ensure it matches a real asset
  FString ResolvedSourcePath = ResolveAssetPath(SourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    // If resolution failed, fall back to original for strict check
    ResolvedSourcePath = SourcePath;
  }

  ResolvedSourcePath = SanitizeProjectRelativePath(ResolvedSourcePath);
  if (ResolvedSourcePath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid resolved sourcePath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(ResolvedSourcePath)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source asset not found: %s"), *SourcePath),
        nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Use the resolved path for the rename operation
  if (UEditorAssetLibrary::RenameAsset(ResolvedSourcePath, DestinationPath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), DestinationPath);

    // Add verification data
    UObject* RenamedAsset = UEditorAssetLibrary::LoadAsset(DestinationPath);
    if (RenamedAsset) {
      McpHandlerUtils::AddVerification(Resp, RenamedAsset);
    }

    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset renamed"), Resp,
                           FString());
  } else {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Failed to rename asset. Check if destination "
                             "'%s' already exists or source is locked."),
                        *DestinationPath),
        nullptr, TEXT("RENAME_FAILED"));
  }
  return true;
}

/**
 * Handles asset deletion requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path' (string) or 'paths' (array of
 * strings).
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleDeleteAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Accept the canonical schema spellings (`assetPath` / `assetPaths`) as well
  // as the legacy `path` / `paths`. Reading only the legacy pair made
  // asset.delete uncallable through the gateway: the schema declares
  // `assetPath`, so a contract-correct call reached here with nothing this
  // handler recognised ("No paths provided"), while sending `assetPaths` was
  // rejected upstream as an undeclared parameter.
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  for (const TCHAR *ArrayField : {TEXT("paths"), TEXT("assetPaths")}) {
    if (Payload->TryGetArrayField(ArrayField, PathsArray) && PathsArray) {
      for (const auto &Val : *PathsArray) {
        if (Val.IsValid() && Val->Type == EJson::String)
          PathsToDelete.Add(Val->AsString());
      }
    }
  }

  for (const TCHAR *StringField : {TEXT("path"), TEXT("assetPath")}) {
    FString SinglePath;
    if (Payload->TryGetStringField(StringField, SinglePath) &&
        !SinglePath.IsEmpty()) {
      PathsToDelete.AddUnique(SinglePath);
    }
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> FailedToDeletePaths;
  TArray<FString> ReferencedPaths;

  // force was declared ("delete even when still referenced") and never read, so
  // a referenced asset was deleted and its referencers broke silently. Without
  // force, an asset something outside this delete references stays in place and
  // is listed under referencedPaths. Folders are deleted whole either way.
  const bool bForce = GetJsonBoolField(Payload, TEXT("force"), false);
  TArray<FString> DeleteSet;
  for (const FString &Path : PathsToDelete) {
    const FString Safe = SanitizeProjectRelativePath(Path);
    if (!Safe.IsEmpty()) { DeleteSet.Add(FPackageName::ObjectPathToPackageName(Safe)); }
  }
  const auto OutsideReferencers = [&DeleteSet](const FString &PackageName) {
    TArray<FAssetIdentifier> Refs;
    FAssetRegistryModule::GetRegistry().GetReferencers(FAssetIdentifier(FName(*PackageName)), Refs,
                                                       UE::AssetRegistry::EDependencyCategory::Package);
    TArray<FString> Outside;
    for (const FAssetIdentifier &Ref : Refs) {
      const FString RefPackage = Ref.PackageName.ToString();
      const bool bDeletedToo = DeleteSet.ContainsByPredicate([&RefPackage](const FString &D) {
        return RefPackage == D || RefPackage.StartsWith(D + TEXT("/"));
      });
      if (!RefPackage.IsEmpty() && !bDeletedToo) { Outside.AddUnique(RefPackage); }
    }
    return Outside;
  };

  for (const FString &Path : PathsToDelete) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty()) {
      FailedToDeletePaths.Add(Path);
      continue;
    }

    // Check if it's a directory first (folder path)
    if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
      // Directory exists - use safe folder deletion with proper cleanup
      // CRITICAL for UE 5.7+: Use McpSafeDeleteFolder instead of UEditorAssetLibrary::DeleteDirectory
      // to prevent crashes during UWorld::CleanupWorld when deleting folders containing
      // AnimBlueprints, IKRigs, IKRetargeters, etc.
      if (McpSafeOperations::McpSafeDeleteFolder(SafePath))
      {
        // McpSafeDeleteFolder performs registry and filesystem verification itself.
        DeletedCount++;
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else if (UEditorAssetLibrary::DoesAssetExist(SafePath) ||
               FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(SafePath))) {
      // The file counts as the asset too: a delete that left the .uasset behind
      // had already dropped it from the registry, so a retry answered "not
      // found" about an asset that came back on the next editor start.
      // And it counts as deleted only once that file is gone.
      const TArray<FString> Referencers = bForce ? TArray<FString>() : OutsideReferencers(FPackageName::ObjectPathToPackageName(SafePath));
      if (Referencers.Num() > 0) {
        ReferencedPaths.Add(FString::Printf(TEXT("%s (referenced by %s)"), *SafePath, *FString::Join(Referencers, TEXT(", "))));
      } else if (McpSafeOperations::McpDeleteAssetAndFile(SafePath)) {
        DeletedCount++;
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else {
      // Asset/directory does not exist
      NotFoundPaths.Add(SafePath);
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

  // Return success only if at least one asset was deleted
  bool bSuccess = DeletedCount > 0;
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("deletedCount"), DeletedCount);
  // Was a hardcoded false, so even a failed delete claimed the asset was gone.
  Resp->SetBoolField(TEXT("existsAfter"), FailedToDeletePaths.Num() > 0 || ReferencedPaths.Num() > 0);

  if (NotFoundPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> NotFoundArray;
    for (const FString& P : NotFoundPaths) {
      NotFoundArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
  }

  if (ReferencedPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> ReferencedArray;
    for (const FString& P : ReferencedPaths) {
      ReferencedArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("referencedPaths"), ReferencedArray);
    Resp->SetStringField(TEXT("referencedHint"), TEXT("Still-referenced assets were kept; pass force:true to delete them anyway (their referencers break)."));
  }

  if (FailedToDeletePaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& P : FailedToDeletePaths) {
      FailedArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("failedToDeletePaths"), FailedArray);
    Resp->SetNumberField(TEXT("failedCount"), FailedToDeletePaths.Num());
  }

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Assets deleted"), Resp, FString());
  } else {
    // Nothing was deleted - determine the reason
    FString ErrorMessage;
    FString ErrorCode;

    if (ReferencedPaths.Num() > 0) {
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d asset(s) are still referenced: %s. Pass force:true to delete anyway."),
                                      ReferencedPaths.Num(), *FString::Join(ReferencedPaths, TEXT("; ")));
      ErrorCode = TEXT("ASSET_REFERENCED");
    } else if (NotFoundPaths.Num() > 0 && FailedToDeletePaths.Num() == 0) {
      // All paths were not found
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found."), NotFoundPaths.Num());
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    } else if (FailedToDeletePaths.Num() > 0 && NotFoundPaths.Num() == 0) {
      // All paths existed but deletion failed
      ErrorMessage = FString::Printf(TEXT("Failed to delete %d asset(s). They may be in use or locked."), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    } else {
      // Mixed: some not found, some failed to delete
      ErrorMessage = FString::Printf(TEXT("No assets deleted. %d path(s) not found, %d failed to delete."),
                                      NotFoundPaths.Num(), FailedToDeletePaths.Num());
      ErrorCode = TEXT("DELETE_FAILED");
    }

    SendAutomationResponse(Socket, RequestId, false, ErrorMessage, Resp, ErrorCode);
  }
  return true;
}

