#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Metadata/McpSeqMetadata.h"
#include "Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h"
#include "Domains/Sequence/McpSeqPathSecurity.h"

// The level sequence a duplicate or rename moves, to DestinationPath under /Game. Anything else is refused here: an
// unwritable path (a copy into, or a rename out of, engine content went through), a missing or non-sequence source,
// and a taken destination, which used to fail as a bare OPERATION_FAILED.
static ULevelSequence *McpResolveSequenceMove(UEngineWireSubsystem *Self, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                              const FString &RequestId, const FString &SourcePath,
                                              const FString &DestinationPath, bool bSourceWritable) {
  FString Resolved, Error;
  if ((bSourceWritable && !McpSequencePathSecurity::ValidateWritableAssetPath(SourcePath, Resolved, Error)) ||
      !McpSequencePathSecurity::ValidateWritableAssetPath(DestinationPath, Resolved, Error)) {
    Self->SendAutomationResponse(Socket, RequestId, false, Error, nullptr, TEXT("SEQUENCE_PATH_NOT_WRITABLE"));
    return nullptr;
  }
  UObject *Source = McpLoadAsset(SourcePath);
  ULevelSequence *Sequence = Cast<ULevelSequence>(Source);
  if (!Sequence) {
    Self->SendAutomationResponse(Socket, RequestId, false,
        Source ? FString::Printf(TEXT("%s is a %s, not a level sequence"), *SourcePath, *Source->GetClass()->GetName())
               : FString::Printf(TEXT("Source sequence not found: %s"), *SourcePath),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return nullptr;
  }
  if (McpAssetExists(DestinationPath)) {
    Self->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("An asset already exists at %s"), *DestinationPath),
                                 nullptr, TEXT("ASSET_ALREADY_EXISTS"));
    return nullptr;
  }
  return Sequence;
}

bool UEngineWireSubsystem::HandleSequenceList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> SequencesArray;

  // path narrows the search to one content folder (recursively); /Game when absent.
  FString Root = TEXT("/Game");
  FString RequestedRoot;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), RequestedRoot) &&
      !RequestedRoot.TrimStartAndEnd().IsEmpty()) {
    const FString RawRoot = RequestedRoot;
    RequestedRoot = McpCanonicalizeContentPath(RequestedRoot, /*bAssumeGameRoot=*/true);
    while (RequestedRoot.Len() > 1 && RequestedRoot.EndsWith(TEXT("/"))) {
      RequestedRoot.LeftChopInline(1);
    }
    // /Game itself is taken as is; any other root goes through the sanitizer, which accepts a bare mounted root.
    Root = RequestedRoot.Equals(TEXT("/Game"), ESearchCase::IgnoreCase)
               ? FString(TEXT("/Game"))
           : RequestedRoot.IsEmpty() ? FString()
                                     : SanitizeProjectRelativePath(RequestedRoot);
    if (Root.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             McpPathRefusalMessage(TEXT("path"), RawRoot),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(ULevelSequence::StaticClass()->GetFName());
#endif
  Filter.bRecursiveClasses = true;
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(FName(*Root));

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> SeqObj = McpHandlerUtils::CreateResultObject();
    // Canonical `/Game/...` asset path: the object-path form (`/Game/X.X`) is a
    // second spelling of the same identity, and the parameter contract asks for
    // the canonical one.
    SeqObj->SetStringField(TEXT("path"), Asset.PackageName.ToString());
    SeqObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
    SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
  }

  Resp->SetArrayField(TEXT("sequences"), SequencesArray);
  Resp->SetNumberField(TEXT("count"), SequencesArray.Num());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Found %d sequences"), SequencesArray.Num()), Resp,
      FString());
  return true;
}

bool UEngineWireSubsystem::HandleSequenceDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SourcePath;
  LocalPayload->TryGetStringField(TEXT("path"), SourcePath);
  FString DestinationPath;
  LocalPayload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_duplicate requires path and destinationPath"), nullptr,
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!DestinationPath.IsEmpty() && !DestinationPath.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(SourcePath);
    DestinationPath =
        FString::Printf(TEXT("%s/%s"), *ParentPath, *DestinationPath);
  }
  // destinationPath is a folder in the published contract; combine it with
  // newName (or the source name) so the copy is not written as an asset named
  // after the folder (dogfood #118).
  FString NewName;
  LocalPayload->TryGetStringField(TEXT("newName"), NewName);
  NewName.TrimStartAndEndInline();
  const bool bDestinationIsFolder =
      UEditorAssetLibrary::DoesDirectoryExist(DestinationPath) ||
      DestinationPath.EndsWith(TEXT("/")) ||
      (!NewName.IsEmpty() && !McpAssetExists(DestinationPath) &&
       !FPaths::GetBaseFilename(DestinationPath).Equals(NewName, ESearchCase::IgnoreCase));
  if (!NewName.IsEmpty()) {
    DestinationPath = (bDestinationIsFolder ? DestinationPath : FPaths::GetPath(DestinationPath)) / NewName;
  } else if (bDestinationIsFolder) {
    DestinationPath = DestinationPath / FPaths::GetBaseFilename(SourcePath);
  }

  if (!McpResolveSequenceMove(this, Socket, RequestId, SourcePath, DestinationPath, false)) {
    return true;
  }
  UObject *DuplicatedSeq =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
  if (DuplicatedSeq) {
    // Saved like a created sequence: the copy lived only in memory and an editor exit lost it.
    const bool bSaved = McpSafeAssetSave(DuplicatedSeq);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("saved"), bSaved);
    Resp->SetStringField(TEXT("sourcePath"), SourcePath);
    Resp->SetStringField(TEXT("destinationPath"), DestinationPath);
    Resp->SetStringField(TEXT("duplicatedPath"), DuplicatedSeq->GetPathName());
    Resp->SetStringField(TEXT("sequencePath"), DuplicatedSeq->GetPathName());
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence duplicated successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to duplicate sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
}

bool UEngineWireSubsystem::HandleSequenceRename(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  FString NewName;
  LocalPayload->TryGetStringField(TEXT("newName"), NewName);
  if (Path.IsEmpty() || NewName.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_rename requires path and newName"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!NewName.IsEmpty() && !NewName.StartsWith(TEXT("/"))) {
    FString ParentPath = FPaths::GetPath(Path);
    NewName = FString::Printf(TEXT("%s/%s"), *ParentPath, *NewName);
  }

  if (!McpResolveSequenceMove(this, Socket, RequestId, Path, NewName, true)) {
    return true;
  }
  if (UEditorAssetLibrary::RenameAsset(Path, NewName)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("oldPath"), Path);
    Resp->SetStringField(TEXT("newName"), NewName);
    // The renamed asset, which the receipt names: the reply carried only the two paths.
    Resp->SetStringField(TEXT("sequencePath"), NewName);
    if (UObject *Renamed = McpLoadAsset(NewName)) {
      McpHandlerUtils::AddVerification(Resp, Renamed);
    }
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence renamed successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to rename sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
}

bool UEngineWireSubsystem::HandleSequenceDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString Path;
  LocalPayload->TryGetStringField(TEXT("path"), Path);
  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("sequence_delete requires path"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!McpAssetExists(Path)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    // A destructive call must say whether it removed anything (dogfood #119).
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Sequence not found: %s"), *Path), Resp,
                           TEXT("NOT_FOUND"));
    return true;
  }

  // UEditorAssetLibrary::DeleteAsset is a force delete: it never checked what plays the sequence (a Level
  // Sequence Actor kept a broken reference) and may clear the undo history. Refuse while anything uses it.
  const FString Package = FPackageName::ObjectPathToPackageName(Path);
  const TArray<FString> Users = McpOutsideReferencers(Package, {Package});
  if (Users.Num() > 0) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));
    SendAutomationResponse(Socket, RequestId, false,
                           FString::Printf(TEXT("Sequence %s is still used by %d asset(s) (referencedBy): remove those uses first, or delete it anyway with manage_asset delete and force."),
                                           *Path, Users.Num()),
                           Resp, TEXT("SEQUENCE_IN_USE"));
    return true;
  }
  if (UEditorAssetLibrary::DeleteAsset(Path)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("deletedPath"), Path);
    Resp->SetBoolField(TEXT("existsAfter"), McpAssetExists(Path));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to delete sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
}
