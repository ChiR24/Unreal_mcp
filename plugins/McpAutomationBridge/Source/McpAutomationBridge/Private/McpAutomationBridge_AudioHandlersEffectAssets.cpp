#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateReverbEffect(
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

  float Density = 1.0f;
  Payload->TryGetNumberField(TEXT("density"), Density);
  float Diffusion = 1.0f;
  Payload->TryGetNumberField(TEXT("diffusion"), Diffusion);
  float Gain = 0.32f;
  Payload->TryGetNumberField(TEXT("gain"), Gain);
  float GainHF = 0.89f;
  Payload->TryGetNumberField(TEXT("gainHF"), GainHF);
  float DecayTime = 1.49f;
  Payload->TryGetNumberField(TEXT("decayTime"), DecayTime);
  float DecayHFRatio = 0.83f;
  Payload->TryGetNumberField(TEXT("decayHFRatio"), DecayHFRatio);
  float ReflectionsGain = 0.05f;
  Payload->TryGetNumberField(TEXT("reflectionsGain"), ReflectionsGain);
  float LateGain = 1.26f;
  Payload->TryGetNumberField(TEXT("lateGain"), LateGain);

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

  UReverbEffect *ReverbEffect = NewObject<UReverbEffect>(Package, FName(*EffectName), RF_Public | RF_Standalone);
  if (!ReverbEffect) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create reverb effect"), TEXT("CREATE_FAILED"));
    return true;
  }

  ReverbEffect->Density = Density;
  ReverbEffect->Diffusion = Diffusion;
  ReverbEffect->Gain = Gain;
  ReverbEffect->GainHF = GainHF;
  ReverbEffect->DecayTime = DecayTime;
  ReverbEffect->DecayHFRatio = DecayHFRatio;
  ReverbEffect->ReflectionsGain = ReflectionsGain;
  ReverbEffect->LateGain = LateGain;

  if (!McpSafeAssetSave(ReverbEffect)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save reverb effect asset"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("effectPath"), ReverbEffect->GetPathName());
  Resp->SetStringField(TEXT("effectName"), EffectName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Reverb effect created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateSourceEffectChain
// -----------------------------------------------------------------------------
// Creates a USoundEffectSourcePresetChain asset.
//
// Payload:  { "chainName": string, "outputPath"?: string }
// Response: { "chainPath": string, "chainName": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateSourceEffectChain(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString ChainName;
  if (!Payload->TryGetStringField(TEXT("chainName"), ChainName) ||
      ChainName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("chainName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Effects");
  }

  FString FullPath;
  if (!McpAudioHandlers::BuildSanitizedAssetPath(OutputPath, ChainName, OutputPath, FullPath)) {
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

  USoundEffectSourcePresetChain *Chain = NewObject<USoundEffectSourcePresetChain>(Package, FName(*ChainName), RF_Public | RF_Standalone);
  if (!Chain) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create source effect chain"), TEXT("CREATE_FAILED"));
    return true;
  }

  if (!McpSafeAssetSave(Chain)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save source effect chain asset"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("chainPath"), Chain->GetPathName());
  Resp->SetStringField(TEXT("chainName"), ChainName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Source effect chain created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleAddSourceEffect
// -----------------------------------------------------------------------------
// Adds an effect entry (EQ, Reverb, or Delay) to a source effect chain.
//
// Payload:  { "chainPath": string, "effectType": string, "effectName"?: string }
// Response: { "chainPath": string, "effectType": string, "effectName": string,
//             "effectIndex": number }
// -----------------------------------------------------------------------------
