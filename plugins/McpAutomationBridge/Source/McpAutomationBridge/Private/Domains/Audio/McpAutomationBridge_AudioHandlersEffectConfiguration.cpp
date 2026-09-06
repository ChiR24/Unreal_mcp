#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

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

  if (EffectType.Equals(TEXT("EQ"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
      TEXT("Use manage_audio_authoring add_source_effect with effectType='EQ' for durable preset creation"),
      TEXT("USE_AUTHORING_PATH"));
    return true;
  } else if (EffectType.Equals(TEXT("Reverb"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
      TEXT("Use manage_audio_authoring add_source_effect with effectType='Reverb' for durable preset creation"),
      TEXT("USE_AUTHORING_PATH"));
    return true;
  } else if (EffectType.Equals(TEXT("Delay"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
      TEXT("Use manage_audio_authoring add_source_effect with effectType='Delay' for durable preset creation"),
      TEXT("USE_AUTHORING_PATH"));
    return true;
  } else {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unknown effect type: %s"), *EffectType),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

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
