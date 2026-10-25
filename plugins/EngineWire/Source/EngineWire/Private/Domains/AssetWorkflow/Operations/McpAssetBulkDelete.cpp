// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h"
#include "Domains/AssetWorkflow/Rename/McpAssetRenameGuard.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"

#include "EditorAssetLibrary.h"
#include "ObjectTools.h"

bool UEngineWireSubsystem::HandleBulkDeleteAssets(
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

  bool bFixupRedirectors = true;
  Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixupRedirectors);

  TArray<FString> AssetPaths;
  if (!McpCollectBulkAssetPaths(*this, RequestId, RequestingSocket, Payload, AssetPaths)) {
    return true;
  }
  if (AssetPaths.Num() == 0) {
    // deleted is a list on every path: a bare 0 here read as a different shape from the reply of a delete.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("deleted"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetNumberField(TEXT("requested"), 0);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No assets found"), Result, FString());
    return true;
  }

  TArray<UObject *> ObjectsToDelete;
  TArray<FString> ValidPaths;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (!SafeAssetPath.IsEmpty() && McpAssetExists(SafeAssetPath)) {
      if (UObject *Asset = McpLoadAsset(SafeAssetPath)) {
        ObjectsToDelete.Add(Asset);
        ValidPaths.Add(SafeAssetPath);
      }
    }
  }

  // The engine deletes nothing at all when any asset of the batch is still referenced (FAssetDeleteModel::
  // CanDelete), and the reply said only BULK_DELETE_FAILED: one used mesh kept a whole folder. An asset
  // something outside this delete references is kept, named with what uses it, and the rest go.
  TArray<FString> DeleteSet;
  for (const FString &Path : ValidPaths) {
    DeleteSet.Add(FPackageName::ObjectPathToPackageName(Path));
  }
  // Until nothing changes: an asset used only by one this keeps is outside-referenced in turn.
  TArray<TSharedPtr<FJsonValue>> Referenced;
  for (bool bKeptOne = true; bKeptOne;) {
    bKeptOne = false;
    for (int32 Index = ValidPaths.Num() - 1; Index >= 0; --Index) {
      const TArray<FString> Users = McpOutsideReferencers(DeleteSet[Index], DeleteSet);
      if (Users.Num() == 0) {
        continue;
      }
      TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
      Row->SetStringField(TEXT("path"), ValidPaths[Index]);
      Row->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));
      Referenced.Add(MakeShared<FJsonValueObject>(Row));
      ObjectsToDelete.RemoveAt(Index);
      ValidPaths.RemoveAt(Index);
      DeleteSet.RemoveAt(Index);
      bKeptOne = true;
    }
  }
  if (ObjectsToDelete.Num() == 0 && Referenced.Num() > 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("deleted"), TArray<TSharedPtr<FJsonValue>>());
    Result->SetArrayField(TEXT("referenced"), Referenced);
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Nothing was deleted: every asset is still used by others (referenced names them). Delete or retarget those first, or use manage_asset delete with force."),
                           Result, TEXT("ASSETS_REFERENCED"));
    return true;
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

  // Never the engine's confirmation dialog: it is modal on the game thread and
  // nobody can answer it over MCP, so the call would hang the editor. The consent
  // grant on this destructive capability is the confirmation.
  int32 DeletedCount =
      ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

  // Scoped to the folders this call actually deleted from. Unscoped, the
  // fixup matched EVERY redirector in the project, so deleting one folder
  // dragged unrelated content through a referencer fixup -- slow, and far
  // wider a mutation than the caller asked for.
  if (bFixupRedirectors && DeletedCount > 0) {
    TSet<FString> Folders;
    for (const FString &Path : ValidPaths) {
      const FString PackageName = FPackageName::ObjectPathToPackageName(Path);
      if (!PackageName.IsEmpty()) {
        Folders.Add(FPackageName::GetLongPackagePath(PackageName));
      }
    }
    for (const FString &Folder : Folders) {
      int32 Found = 0;
      int32 Fixed = 0;
      McpAssetRename::FixupRedirectorsIn(Folder, Found, Fixed);
    }
  }

  // ObjectTools::DeleteObjects returns a count, not a list, and reporting
  // every attempted path as deleted told callers an asset was gone while it
  // was still on disk -- worse than saying nothing, because a caller that
  // trusts it skips the retry. Ask whether each one is actually gone.
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  TArray<TSharedPtr<FJsonValue>> RemainingArray;
  for (const FString &Path : ValidPaths) {
    const bool bGone = !McpAssetExists(Path);
    (bGone ? DeletedArray : RemainingArray)
        .Add(MakeShared<FJsonValueString>(Path));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), DeletedCount > 0);
  Result->SetArrayField(TEXT("deleted"), DeletedArray);
  Result->SetNumberField(TEXT("requested"), ObjectsToDelete.Num() + Referenced.Num());
  if (RemainingArray.Num() > 0) {
    Result->SetArrayField(TEXT("remaining"), RemainingArray);
  }
  if (Referenced.Num() > 0) {
    Result->SetArrayField(TEXT("referenced"), Referenced);
  }

  FString Message = FString::Printf(TEXT("Deleted %d of %d assets"), DeletedCount, ObjectsToDelete.Num() + Referenced.Num());
  if (Referenced.Num() > 0) {
    Message += FString::Printf(TEXT("; %d kept because others use them (referenced)"), Referenced.Num());
  }
  if (DeletedCount == 0) {
    // Nothing went although nothing on disk references them: something in memory still holds them.
    Message = TEXT("The engine deleted nothing: something loaded (an open level or asset editor) still holds them.");
  }
  SendAutomationResponse(RequestingSocket, RequestId, DeletedCount > 0, Message, Result,
                         DeletedCount > 0 ? FString() : TEXT("BULK_DELETE_FAILED"));
  return true;
}
