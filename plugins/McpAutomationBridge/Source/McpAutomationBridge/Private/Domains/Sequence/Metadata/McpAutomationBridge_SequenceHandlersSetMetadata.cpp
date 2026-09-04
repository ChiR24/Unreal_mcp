// Sequence metadata WRITE. Lives in its own subdirectory because both CI
// ceilings are already met next door: McpAutomationBridge_SequenceHandlersAssetLibrary.cpp
// (which owns the matching read) sits at the 250 pure-line limit, and
// Private/Domains/Sequence/ is at the 25-file folder limit.

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Domains/Sequence/Metadata/McpAutomationBridge_SequenceMetadata.h"
#include "Safety/McpSafeOperations.h"

namespace {
// Same bounded coercion the Blueprint metadata handler uses: only scalars become
// tag values, so a nested object is skipped rather than silently stringified into
// a shape no reader expects.
bool CoerceMetadataValue(const TSharedPtr<FJsonValue> &Value, FString &OutValue) {
  if (!Value.IsValid()) return false;
  if (Value->Type == EJson::String) {
    OutValue = Value->AsString();
  } else if (Value->Type == EJson::Boolean) {
    OutValue = Value->AsBool() ? TEXT("true") : TEXT("false");
  } else if (Value->Type == EJson::Number) {
    OutValue = FString::Printf(TEXT("%g"), Value->AsNumber());
  } else {
    return false;
  }
  return true;
}
}

bool UMcpAutomationBridgeSubsystem::HandleSequenceSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  FString SeqPath = ResolveSequencePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_metadata requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
  // Accept the documented `metadata` object (or `tags`) and the single-pair
  // `key`/`value` spelling that callers reach for first (dogfood #127).
  TArray<TPair<FString, FString>> Pending;
  const TSharedPtr<FJsonObject> *MetadataObj = nullptr;
  if ((LocalPayload->TryGetObjectField(TEXT("metadata"), MetadataObj) ||
       LocalPayload->TryGetObjectField(TEXT("tags"), MetadataObj)) &&
      MetadataObj && (*MetadataObj).IsValid()) {
    for (const auto &Pair : (*MetadataObj)->Values) {
      FString MetaValue;
      // UE 5.8 keys Values by UE::FSharedString; *Pair.Key is const TCHAR* on
      // both versions, so FString gets built either way.
      if (CoerceMetadataValue(Pair.Value, MetaValue)) {
        Pending.Emplace(FString(*Pair.Key), MetaValue);
      }
    }
  }
  FString Key;
  FString SingleValue;
  if (LocalPayload->TryGetStringField(TEXT("key"), Key) && !Key.IsEmpty() &&
      CoerceMetadataValue(LocalPayload->TryGetField(TEXT("value")), SingleValue)) {
    Pending.Emplace(Key, SingleValue);
  }
  if (Pending.Num() == 0) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_metadata requires a metadata object or key + value"),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Sequence not found"),
                           nullptr, TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TArray<TSharedPtr<FJsonValue>> WrittenKeys;
  for (const TPair<FString, FString> &Pair : Pending) {
    // Package metadata (UMetaData before 5.6, FMetaData after) via the
    // version-neutral editor library; get_metadata reads the same store.
    UEditorAssetLibrary::SetMetadataTag(SeqObj, FName(*Pair.Key), Pair.Value);
    WrittenKeys.Add(MakeShared<FJsonValueString>(Pair.Key));
  }
  SeqObj->MarkPackageDirty();
  const bool bSaved = McpSafeOperations::McpSafeAssetSave(SeqObj);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetArrayField(TEXT("metadataSet"), WrittenKeys);
  Resp->SetObjectField(TEXT("metadata"),
                       McpSequenceMetadata::BuildMetadataObject(SeqObj));
  Resp->SetBoolField(TEXT("saved"), bSaved);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Sequence metadata set"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("sequence_set_metadata requires editor build."),
                         nullptr, TEXT("NOT_AVAILABLE"));
  return true;
#endif
}