#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Metadata/McpAutomationBridge_SequenceMetadata.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> SequencesArray;

  // path narrows the search to one content folder (recursively); /Game when absent.
  FString Root = TEXT("/Game");
  FString RequestedRoot;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), RequestedRoot) &&
      !RequestedRoot.TrimStartAndEnd().IsEmpty()) {
    RequestedRoot = McpCanonicalizeContentPath(RequestedRoot, /*bAssumeGameRoot=*/true);
    while (RequestedRoot.Len() > 1 && RequestedRoot.EndsWith(TEXT("/"))) {
      RequestedRoot.LeftChopInline(1);
    }
    // The sanitizer wants a folder below a root, so the bare root is taken as is.
    Root = RequestedRoot.Equals(TEXT("/Game"), ESearchCase::IgnoreCase)
               ? FString(TEXT("/Game"))
           : RequestedRoot.IsEmpty() ? FString()
                                     : SanitizeProjectRelativePath(RequestedRoot);
    if (Root.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("path must be a content folder under a mounted "
                                  "root, for example the project Game folder"),
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

bool UMcpAutomationBridgeSubsystem::HandleSequenceDuplicate(
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

  UObject *SourceSeq = McpLoadAsset(SourcePath);
  if (!SourceSeq) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source sequence not found: %s"), *SourcePath),
        nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  UObject *DuplicatedSeq =
      UEditorAssetLibrary::DuplicateAsset(SourcePath, DestinationPath);
  if (DuplicatedSeq) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
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

bool UMcpAutomationBridgeSubsystem::HandleSequenceRename(
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

  if (UEditorAssetLibrary::RenameAsset(Path, NewName)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("oldPath"), Path);
    Resp->SetStringField(TEXT("newName"), NewName);
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

bool UMcpAutomationBridgeSubsystem::HandleSequenceDelete(
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
