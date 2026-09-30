// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowBulkSelection.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"

#include "EditorAssetLibrary.h"
#include "ObjectTools.h"

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
