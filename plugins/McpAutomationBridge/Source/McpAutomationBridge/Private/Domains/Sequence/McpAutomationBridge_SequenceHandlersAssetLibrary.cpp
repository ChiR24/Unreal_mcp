#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Metadata/McpAutomationBridge_SequenceMetadata.h"

bool UMcpAutomationBridgeSubsystem::HandleSequenceList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> SequencesArray;

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
  Filter.PackagePaths.Add(FName("/Game"));

  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  for (const FAssetData &Asset : AssetList) {
    TSharedPtr<FJsonObject> SeqObj = McpHandlerUtils::CreateResultObject();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    SeqObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
#else
    SeqObj->SetStringField(TEXT("path"), FString::Printf(TEXT("%s.%s"), *Asset.PackageName.ToString(), *Asset.AssetName.ToString()));
#endif
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
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_list requires editor build."), nullptr,
                         TEXT("NOT_AVAILABLE"));
  return true;
#endif
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
#if WITH_EDITOR
  const bool bDestinationIsFolder =
      UEditorAssetLibrary::DoesDirectoryExist(DestinationPath) ||
      DestinationPath.EndsWith(TEXT("/")) ||
      (!NewName.IsEmpty() && !UEditorAssetLibrary::DoesAssetExist(DestinationPath) &&
       !FPaths::GetBaseFilename(DestinationPath).Equals(NewName, ESearchCase::IgnoreCase));
  if (!NewName.IsEmpty()) {
    DestinationPath = (bDestinationIsFolder ? DestinationPath : FPaths::GetPath(DestinationPath)) / NewName;
  } else if (bDestinationIsFolder) {
    DestinationPath = DestinationPath / FPaths::GetBaseFilename(SourcePath);
  }
#endif

#if WITH_EDITOR
  UObject *SourceSeq = UEditorAssetLibrary::LoadAsset(SourcePath);
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
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_duplicate requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
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

#if WITH_EDITOR
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
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_rename requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
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
#if WITH_EDITOR
  if (!UEditorAssetLibrary::DoesAssetExist(Path)) {
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
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Sequence deleted successfully"), Resp,
                           FString());
    return true;
  }
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("Failed to delete sequence"), nullptr,
                         TEXT("OPERATION_FAILED"));
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_delete requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
