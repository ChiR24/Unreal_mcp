#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueVoice(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString VoiceName;
  if (!Payload->TryGetStringField(TEXT("voiceName"), VoiceName) ||
      VoiceName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voiceName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  // Parse gender setting
  FString GenderStr;
  TEnumAsByte<EGrammaticalGender::Type> Gender = EGrammaticalGender::Masculine;
  if (Payload->TryGetStringField(TEXT("gender"), GenderStr)) {
    Gender = GenderStr.Equals(TEXT("Female"), ESearchCase::IgnoreCase)
                 ? EGrammaticalGender::Feminine
                 : EGrammaticalGender::Masculine;
  }

  // Parse pluralization setting
  FString PluralStr;
  TEnumAsByte<EGrammaticalNumber::Type> Plurality = EGrammaticalNumber::Singular;
  if (Payload->TryGetStringField(TEXT("pluralization"), PluralStr)) {
    Plurality = PluralStr.Equals(TEXT("Plural"), ESearchCase::IgnoreCase)
                    ? EGrammaticalNumber::Plural
                    : EGrammaticalNumber::Singular;
  }

  FString FullPath;
  if (!McpAudioHandlers::BuildSanitizedAssetPath(OutputPath, VoiceName, OutputPath, FullPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid outputPath"), TEXT("INVALID_PATH"));
    return true;
  }
  FString PackageName = FullPath;

  UPackage *Package = CreatePackage(*PackageName);
  if (!Package) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create package"), TEXT("CREATE_FAILED"));
    return true;
  }

  UDialogueVoice *NewVoice = NewObject<UDialogueVoice>(Package, FName(*VoiceName), RF_Public | RF_Standalone);
  if (!NewVoice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue voice"), TEXT("CREATE_FAILED"));
    return true;
  }

  // UE 5.7: VoiceName removed, Gender uses EGrammaticalGender, bIsPlural replaced with Plurality
  NewVoice->Gender = Gender;
  NewVoice->Plurality = Plurality;

  if (!McpSafeAssetSave(NewVoice)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save dialogue voice asset"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("voicePath"), NewVoice->GetPathName());
  Resp->SetStringField(TEXT("voiceName"), VoiceName);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue voice created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleCreateDialogueWave
// -----------------------------------------------------------------------------
// Creates a UDialogueWave asset with a sound wave and initial context mapping.
//
// VERSION COMPATIBILITY:
// - UE 5.0-5.6: FDialogueContextMapping uses DialogueVoice for speaker
// - UE 5.7: DialogueVoice renamed to Speaker
//
// Payload:  { "waveName": string, "soundPath": string, "outputPath"?: string }
// Response: { "wavePath": string, "waveName": string, "soundPath": string }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleCreateDialogueWave(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WaveName;
  if (!Payload->TryGetStringField(TEXT("waveName"), WaveName) ||
      WaveName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("waveName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SoundPath;
  if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
      SoundPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  USoundWave *SoundWave = Cast<USoundWave>(McpAudioHandlers::ResolveSoundAsset(SoundPath));
  if (!SoundWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("soundPath must reference a SoundWave, not a SoundCue or other sound type"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString OutputPath;
  if (!Payload->TryGetStringField(TEXT("outputPath"), OutputPath) ||
      OutputPath.IsEmpty()) {
    OutputPath = TEXT("/Game/Audio/Dialogues");
  }

  FString FullPath;
  if (!McpAudioHandlers::BuildSanitizedAssetPath(OutputPath, WaveName, OutputPath, FullPath)) {
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

  UDialogueWave *DialogueWave = NewObject<UDialogueWave>(Package, FName(*WaveName), RF_Public | RF_Standalone);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create dialogue wave"), TEXT("CREATE_FAILED"));
    return true;
  }

  FDialogueContextMapping Context;
  Context.Context.Speaker = nullptr;
  Context.SoundWave = SoundWave;
  DialogueWave->ContextMappings.Add(Context);

  if (!McpSafeAssetSave(DialogueWave)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save dialogue wave asset"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("waveName"), WaveName);
  Resp->SetStringField(TEXT("soundPath"), SoundPath);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue wave created"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// -----------------------------------------------------------------------------
// HandleSetDialogueContext
// -----------------------------------------------------------------------------
// Sets the speaker voice on a dialogue wave's context mapping.
//
// VERSION COMPATIBILITY:
// - UE 5.0-5.6: Uses DialogueVoice property
// - UE 5.7: DialogueVoice renamed to Speaker
//
// Payload:  { "wavePath": string, "voicePath": string, "contextIndex"?: number }
// Response: { "wavePath": string, "voicePath": string, "contextIndex": number }
// -----------------------------------------------------------------------------
bool UMcpAutomationBridgeSubsystem::HandleSetDialogueContext(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  FString WavePath;
  if (!Payload->TryGetStringField(TEXT("wavePath"), WavePath) ||
      WavePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("wavePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueWave *DialogueWave = LoadObject<UDialogueWave>(nullptr, *WavePath);
  if (!DialogueWave) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue wave not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString VoicePath;
  if (!Payload->TryGetStringField(TEXT("voicePath"), VoicePath) ||
      VoicePath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("voicePath required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UDialogueVoice *Voice = LoadObject<UDialogueVoice>(nullptr, *VoicePath);
  if (!Voice) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Dialogue voice not found"), TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  int32 ContextIndex = 0;
  Payload->TryGetNumberField(TEXT("contextIndex"), ContextIndex);

  if (ContextIndex < 0 || ContextIndex >= DialogueWave->ContextMappings.Num()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Invalid context index"), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  // UE 5.7: DialogueVoice renamed to Speaker
  DialogueWave->ContextMappings[ContextIndex].Context.Speaker = Voice;
  DialogueWave->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("wavePath"), DialogueWave->GetPathName());
  Resp->SetStringField(TEXT("voicePath"), VoicePath);
  Resp->SetNumberField(TEXT("contextIndex"), ContextIndex);
  SendAutomationResponse(RequestingSocket, RequestId, true,
                         TEXT("Dialogue context set"), Resp);
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

// =============================================================================
// Section 7: Audio Effects Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// HandleCreateReverbEffect
// -----------------------------------------------------------------------------
// Creates a UReverbEffect asset with configurable reverb parameters.
//
// Payload:  { "effectName": string, "outputPath"?: string, "density"?: number,
//             "diffusion"?: number, "gain"?: number, "gainHF"?: number,
//             "decayTime"?: number, "decayHFRatio"?: number,
//             "reflectionsGain"?: number, "lateGain"?: number }
// Response: { "effectPath": string, "effectName": string }
// -----------------------------------------------------------------------------
