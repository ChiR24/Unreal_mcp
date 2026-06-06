#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleAddSourceEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainPath;
  if (!Payload->TryGetStringField(TEXT("chainPath"), ChainPath) ||
      ChainPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundEffectSourcePresetChain *Chain = LoadObject<USoundEffectSourcePresetChain>(nullptr, *ChainPath);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Source effect chain not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectType required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString EffectName;
  Payload->TryGetStringField(TEXT("effectName"), EffectName);
  if (EffectName.IsEmpty()) {
    EffectName = FString::Printf(TEXT("Effect_%d"), Chain->Chain.Num());
  }

  FSourceEffectChainEntry Entry;
  Entry.bBypass = false;

  if (EffectType.Equals(TEXT("EQ"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *EQPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = EQPreset;
  } else if (EffectType.Equals(TEXT("Reverb"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *ReverbPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = ReverbPreset;
  } else if (EffectType.Equals(TEXT("Delay"), ESearchCase::IgnoreCase)) {
    USoundEffectSourcePreset *DelayPreset = NewObject<USoundEffectSourcePreset>();
    Entry.Preset = DelayPreset;
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown effect type: %s"), *EffectType),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  Chain->Chain.Add(Entry);
  Chain->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("effectType"), EffectType);
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetNumberField(TEXT("effectIndex"), Chain->Chain.Num() - 1);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect added to chain"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateSubmixEffect
// -----------------------------------------------------------------------------
// Creates a USoundEffectSubmixPreset asset.
//
// Payload:  { "effectName": string, "outputPath"?: string,
//             "effectType"?: string }
// Response: { "effectPath": string, "effectName": string,
//             "effectType": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateSubmixEffect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString EffectName;
  if (!Payload->TryGetStringField(TEXT("effectName"), EffectName) ||
      EffectName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("effectName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString EffectType;
  if (!Payload->TryGetStringField(TEXT("effectType"), EffectType) ||
      EffectType.IsEmpty()) {
    EffectType = TEXT("Reverb");
  }

  FString FullPath;
  if (!McpAudioHandlers::BuildSanitizedAssetPath(OutputPath, EffectName, OutputPath, FullPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid outputPath"), TEXT("INVALID_PATH"));
    return true;
  }

  UPackage *Package = CreatePackage(*FullPath);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  USoundEffectSubmixPreset *SubmixEffect = NewObject<USoundEffectSubmixPreset>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!SubmixEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create submix effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  if (!McpSafeAssetSave(SubmixEffect)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save submix effect asset"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("effectPath"), SubmixEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  Resp->SetStringField(TEXT("effectType"), EffectType);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Submix effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}
