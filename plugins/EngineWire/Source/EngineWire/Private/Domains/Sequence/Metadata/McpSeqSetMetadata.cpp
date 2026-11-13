// Sequence metadata WRITE (sequence_set_metadata); McpSeqGetMetadata.cpp beside it holds the read.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "Domains/Sequence/McpSeqEditorSupport.h"
#include "Domains/Sequence/Metadata/McpSeqMetadata.h"
#include "Safety/McpSafeOperations.h"

bool UEngineWireSubsystem::HandleSequenceSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SeqPath = McpSequence::ResolvePathOrReply(this, RequestId, Socket, Payload, TEXT("sequence_set_metadata"));
  if (SeqPath.IsEmpty()) {
    return true;
  }
  // Accept the documented `metadata` object and the single-pair `key`/`value`
  // spelling that callers reach for first.
  TArray<TPair<FString, FString>> Pending;
  const TSharedPtr<FJsonObject> *MetadataObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("metadata"), MetadataObj) &&
      MetadataObj && (*MetadataObj).IsValid()) {
    for (const auto &Pair : (*MetadataObj)->Values) {
      FString MetaValue;
      // UE 5.8 keys Values by UE::FSharedString; *Pair.Key is const TCHAR* on
      // both versions, so FString gets built either way.
      if (McpJsonScalarToString(Pair.Value, MetaValue)) {
        Pending.Emplace(FString(*Pair.Key), MetaValue);
      }
    }
  }
  FString Key;
  FString SingleValue;
  if (Payload->TryGetStringField(TEXT("key"), Key) && !Key.IsEmpty() &&
      McpJsonScalarToString(Payload->TryGetField(TEXT("value")), SingleValue)) {
    Pending.Emplace(Key, SingleValue);
  }
  if (Pending.Num() == 0) {
    SendAutomationError(
        Socket, RequestId,
        TEXT("sequence_set_metadata requires a metadata object or key + value"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  UObject *SeqObj = McpLoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationError(Socket, RequestId, TEXT("Sequence not found"),
                        TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TArray<FString> WrittenKeys;
  for (const TPair<FString, FString> &Pair : Pending) {
    // Package metadata (UMetaData before 5.6, FMetaData after) via the
    // version-neutral editor library; get_metadata reads the same store.
    UEditorAssetLibrary::SetMetadataTag(SeqObj, FName(*Pair.Key), Pair.Value);
    WrittenKeys.Add(Pair.Key);
  }
  SeqObj->MarkPackageDirty();
  const bool bSaved = McpSafeOperations::McpSafeAssetSave(SeqObj);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetArrayField(TEXT("metadataSet"), McpHandlerUtils::ToJsonStringArray(WrittenKeys));
  Resp->SetObjectField(TEXT("metadata"),
                       McpSequenceMetadata::BuildMetadataObject(SeqObj));
  Resp->SetBoolField(TEXT("saved"), bSaved);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Sequence metadata set"), Resp, FString());
  return true;
}