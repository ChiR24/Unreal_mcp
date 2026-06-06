// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleRenameAsset(
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
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Display,
        TEXT(
            "HandleRenameAsset: Auto-resolved simple name destination to '%s'"),
        *DestinationPath);
  }

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
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleMoveAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // Move is essentially rename in Unreal
  return HandleRenameAsset(RequestId, Payload, Socket);
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
#if WITH_EDITOR
  // Support both single 'path' and array 'paths'
  TArray<FString> PathsToDelete;
  const TArray<TSharedPtr<FJsonValue>> *PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const auto &Val : *PathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String)
        PathsToDelete.Add(Val->AsString());
    }
  }

  FString SinglePath;
  if (Payload->TryGetStringField(TEXT("path"), SinglePath) &&
      !SinglePath.IsEmpty()) {
    PathsToDelete.Add(SinglePath);
  }

  if (PathsToDelete.Num() == 0) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("No paths provided"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 DeletedCount = 0;
  TArray<FString> NotFoundPaths;
  TArray<FString> FailedToDeletePaths;

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
      if (McpSafeOperations::McpSafeDeleteFolder(SafePath, true))
      {
        // McpSafeDeleteFolder performs registry and filesystem verification itself.
        DeletedCount++;
      } else {
        FailedToDeletePaths.Add(SafePath);
      }
    } else if (UEditorAssetLibrary::DoesAssetExist(SafePath)) {
      // Asset exists - attempt to delete it
      if (UEditorAssetLibrary::DeleteAsset(SafePath)) {
        // Verify the asset was actually deleted
        if (!UEditorAssetLibrary::DoesAssetExist(SafePath)) {
          DeletedCount++;
        } else {
          // Delete returned true but asset still exists
          FailedToDeletePaths.Add(SafePath);
        }
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
  Resp->SetBoolField(TEXT("existsAfter"), false);

  if (NotFoundPaths.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> NotFoundArray;
    for (const FString& P : NotFoundPaths) {
      NotFoundArray.Add(MakeShared<FJsonValueString>(P));
    }
    Resp->SetArrayField(TEXT("notFoundPaths"), NotFoundArray);
    Resp->SetNumberField(TEXT("notFoundCount"), NotFoundPaths.Num());
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

    if (NotFoundPaths.Num() > 0 && FailedToDeletePaths.Num() == 0) {
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
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles folder creation requests.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'path'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
