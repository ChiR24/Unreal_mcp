// Sequence metadata WRITE. Lives in its own subdirectory because both CI
// ceilings are already met next door: McpAutomationBridge_SequenceHandlersAssetLibrary.cpp
// (which owns the matching read) sits at the 250 pure-line limit, and
// Private/Domains/Sequence/ is at the 25-file folder limit.

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"
#include "Safety/McpSafeOperations.h"

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
  const TSharedPtr<FJsonObject> *MetadataObj = nullptr;
  if (!LocalPayload->TryGetObjectField(TEXT("metadata"), MetadataObj) ||
      !MetadataObj || !(*MetadataObj).IsValid()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_set_metadata requires a metadata object"), nullptr,
        TEXT("INVALID_ARGUMENT"));
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
  for (const auto &Pair : (*MetadataObj)->Values) {
    if (!Pair.Value.IsValid()) {
      continue;
    }
    // Same bounded coercion the Blueprint metadata handler uses: only scalars
    // become tag values, so a nested object is skipped rather than silently
    // stringified into a shape no reader expects.
    FString MetaValue;
    if (Pair.Value->Type == EJson::String) {
      MetaValue = Pair.Value->AsString();
    } else if (Pair.Value->Type == EJson::Boolean) {
      MetaValue = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
    } else if (Pair.Value->Type == EJson::Number) {
      MetaValue = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
    } else {
      continue;
    }
    UEditorAssetLibrary::SetMetadataTag(SeqObj, FName(*Pair.Key), MetaValue);
    WrittenKeys.Add(MakeShared<FJsonValueString>(Pair.Key));
  }
  const bool bSaved = McpSafeOperations::McpSafeAssetSave(SeqObj);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetArrayField(TEXT("metadataSet"), WrittenKeys);
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