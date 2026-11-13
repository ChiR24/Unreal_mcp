#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "Domains/Sequence/Metadata/McpSeqMetadata.h"
#include "Domains/Sequence/McpSeqEditorSupport.h"

namespace McpSequenceMetadata {
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

bool HandleGetMetadata(UEngineWireSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SeqPath;
  UObject *SeqObj = McpSequence::LoadAssetOrReply(Subsystem, RequestId, Socket, Payload,
                                                  TEXT("sequence_get_metadata"), SeqPath);
  if (!SeqObj) {
    return true;
  }
  TSharedPtr<FJsonObject> Metadata = BuildMetadataObject(SeqObj);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("path"), SeqPath);
  Resp->SetStringField(TEXT("name"), SeqObj->GetName());
  Resp->SetStringField(TEXT("class"), SeqObj->GetClass()->GetName());
  Resp->SetObjectField(TEXT("metadata"), Metadata);
  Resp->SetNumberField(TEXT("metadataCount"), Metadata->Values.Num());
  FString Key;
  if (Payload->TryGetStringField(TEXT("key"), Key) && !Key.IsEmpty()) {
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
}
}
