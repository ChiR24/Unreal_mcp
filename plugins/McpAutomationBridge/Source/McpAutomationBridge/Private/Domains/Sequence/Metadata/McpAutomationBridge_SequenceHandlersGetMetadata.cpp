// Sequence metadata READ. Lives next to the write so both sides of the key/value
// contract share one reader; the older path/name/class-only reader in
// McpAutomationBridge_SequenceHandlersAssetLibrary.cpp is no longer routed.

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Sequence/Metadata/McpAutomationBridge_SequenceMetadata.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceHandlersEditorSupport.h"

namespace McpSequenceMetadata {
#if WITH_EDITOR
TSharedPtr<FJsonObject> BuildMetadataObject(UObject *Asset) {
  TSharedPtr<FJsonObject> Metadata = McpHandlerUtils::CreateResultObject();
  if (!Asset) return Metadata;
  // UEditorAssetLibrary reads the package metadata map for the object (UMetaData
  // before 5.6, FMetaData from 5.6) - the same store SetMetadataTag writes.
  TMap<FName, FString> Tags = UEditorAssetLibrary::GetMetadataTagValues(Asset);
  Tags.KeySort(FNameLexicalLess());
  for (const TPair<FName, FString> &Tag : Tags) {
    Metadata->SetStringField(Tag.Key.ToString(), Tag.Value);
  }
  return Metadata;
}
#endif

bool HandleGetMetadata(UMcpAutomationBridgeSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> LocalPayload =
      Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  const FString SeqPath = McpSequence::ResolvePath(LocalPayload);
  if (SeqPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("sequence_get_metadata requires a sequence path"), nullptr,
        TEXT("INVALID_SEQUENCE"));
    return true;
  }
#if WITH_EDITOR
  UObject *SeqObj = UEditorAssetLibrary::LoadAsset(SeqPath);
  if (!SeqObj) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
                                      TEXT("Sequence not found"), nullptr,
                                      TEXT("INVALID_SEQUENCE"));
    return true;
  }
  TSharedPtr<FJsonObject> Metadata = BuildMetadataObject(SeqObj);
  // Dogfood #127: set_metadata stores pairs as editor metadata tags; read them back here.
  for (const TPair<FName, FString>& Tag : UEditorAssetLibrary::GetMetadataTagValues(SeqObj)) {
    Metadata->SetStringField(Tag.Key.ToString(), Tag.Value);
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetStringField(TEXT("name"), SeqObj->GetName());
  Resp->SetStringField(TEXT("class"), SeqObj->GetClass()->GetName());
  Resp->SetObjectField(TEXT("metadata"), Metadata);
  Resp->SetNumberField(TEXT("metadataCount"), Metadata->Values.Num());
  FString Key;
  if (LocalPayload->TryGetStringField(TEXT("key"), Key) && !Key.IsEmpty()) {
    FString Value;
    const bool bFound = Metadata->TryGetStringField(Key, Value);
    Resp->SetStringField(TEXT("key"), Key);
    Resp->SetBoolField(TEXT("found"), bFound);
    if (bFound) Resp->SetStringField(TEXT("value"), Value);
  }
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Sequence metadata retrieved"), Resp,
                                    FString());
  return true;
#else
  Subsystem->SendAutomationResponse(
      Socket, RequestId, false,
      TEXT("sequence_get_metadata requires editor build."), nullptr,
      TEXT("NOT_AVAILABLE"));
  return true;
#endif
}
}
