#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SequenceHandlersEditorSupport.h"

FString McpSequence::ResolvePath(const TSharedPtr<FJsonObject> &Payload) {
  FString Path;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("path"), Path) &&
      !Path.IsEmpty()) {
#if WITH_EDITOR
    if (UEditorAssetLibrary::DoesAssetExist(Path)) {
      UObject *Obj = UEditorAssetLibrary::LoadAsset(Path);
      if (Obj) {
        return Obj->GetPathName();
      }
    }
#endif
    return Path;
  }
  if (!GCurrentSequencePath.IsEmpty())
    return GCurrentSequencePath;
  return FString();
}

FString UMcpAutomationBridgeSubsystem::ResolveSequencePath(
    const TSharedPtr<FJsonObject> &Payload) {
  return McpSequence::ResolvePath(Payload);
}

TSharedPtr<FJsonObject>
UMcpAutomationBridgeSubsystem::EnsureSequenceEntry(const FString &SeqPath) {
  if (SeqPath.IsEmpty())
    return nullptr;
  if (TSharedPtr<FJsonObject> *Found = GSequenceRegistry.Find(SeqPath))
    return *Found;
  TSharedPtr<FJsonObject> NewObj = McpHandlerUtils::CreateResultObject();
  NewObj->SetStringField(TEXT("sequencePath"), SeqPath);
  GSequenceRegistry.Add(SeqPath, NewObj);
  return NewObj;
}
