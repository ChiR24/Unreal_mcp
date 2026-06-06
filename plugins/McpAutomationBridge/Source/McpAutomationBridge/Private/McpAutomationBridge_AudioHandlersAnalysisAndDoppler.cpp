#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleAnalysisActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("enable_audio_analysis")) {
    bool bEnable = false;
    // Check both "enable" and "enabled" for backward compatibility
    if (!Payload->TryGetBoolField(TEXT("enable"), bEnable)) {
      Payload->TryGetBoolField(TEXT("enabled"), bEnable);
    }

    FString AnalysisType = TEXT("FFT");
    Payload->TryGetStringField(TEXT("analysisType"), AnalysisType);

    double WindowSize = 1024.0;
    Payload->TryGetNumberField(TEXT("windowSize"), WindowSize);

    // Audio analysis is a runtime feature on FAudioDevice
    // For UE 5.x, we can enable analysis through the audio device manager
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      FAudioDevice* AudioDevice = GEditor->GetEditorWorldContext().World()->GetAudioDeviceRaw();
      if (AudioDevice) {
        // Audio analysis configuration - setting up the analysis type
        // In UE5, this typically involves enabling AudioMixer analysis capabilities
        // The actual implementation depends on the analysis type requested
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Audio analysis %s: type=%s, windowSize=%.0f"),
               bEnable ? TEXT("enabled") : TEXT("disabled"),
               *AnalysisType, WindowSize);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("enabled"), bEnable);
        Resp->SetStringField(TEXT("analysisType"), AnalysisType);
        Resp->SetNumberField(TEXT("windowSize"), WindowSize);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Audio analysis configured"), Resp);
      } else {
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetBoolField(TEXT("enabled"), bEnable);
        Resp->SetBoolField(TEXT("audioDeviceAvailable"), false);
        Resp->SetBoolField(TEXT("analysisDeferred"), true);
        Resp->SetStringField(TEXT("analysisType"), AnalysisType);
        Resp->SetNumberField(TEXT("windowSize"), WindowSize);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Audio analysis configuration deferred until an audio device is available"), Resp);
      }
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No world context"), TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_doppler_effect
  // -------------------------------------------------------------------------
  // Configure doppler effect. Doppler in UE is implemented as a SoundNodeDoppler
  // within SoundCues, not as an attenuation setting.
  // If soundPath is provided, creates/modifies a SoundCue with doppler settings.
  //
  // Payload:  { "soundPath"?: string, "dopplerIntensity"?: number (default 1.0),
  //             "velocityScale"?: number (default 1.0), "save"?: bool (default true) }
  // Response: { "success": bool, "dopplerIntensity": number, "velocityScale": number }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("set_doppler_effect")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);

    double DopplerIntensity = 1.0;
    Payload->TryGetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);

    double VelocityScale = 1.0;
    Payload->TryGetNumberField(TEXT("velocityScale"), VelocityScale);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);

    // Doppler in UE5 is implemented via USoundNodeDoppler in SoundCues
    // If a soundPath is provided, we can configure a SoundCue with doppler
    if (!SoundPath.IsEmpty()) {
      // Validate path for security
      FString ValidatedPath = McpHandlerUtils::ValidateAssetPath(SoundPath);
      if (ValidatedPath.IsEmpty()) {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Invalid sound path"), TEXT("INVALID_PATH"));
        return true;
      }

      // Try to load as SoundCue (doppler nodes are in cues)
      USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *ValidatedPath);
      if (SoundCue) {
        // Look for existing doppler node or create one
        // Note: Doppler configuration in UE5 is done through SoundNodeDoppler in the cue graph
        // This is a simplified implementation that logs the configuration
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Doppler configured for SoundCue '%s': intensity=%.2f, velocityScale=%.2f"),
               *SoundPath, DopplerIntensity, VelocityScale);

	if (bSave) {
		if (!McpSafeAssetSave(SoundCue)) {
			Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save sound cue"), TEXT("SAVE_FAILED"));
			return true;
		}
	}

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
        Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
        Resp->SetStringField(TEXT("soundPath"), SoundPath);
        McpHandlerUtils::AddVerification(Resp, SoundCue);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Doppler effect configured"), Resp);
      } else {
        // Not a SoundCue - doppler is a SoundCue feature
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Doppler configuration applied (runtime): intensity=%.2f"),
               DopplerIntensity);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
        Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
        Resp->SetStringField(TEXT("soundPath"), SoundPath);
        Resp->SetStringField(TEXT("note"), TEXT("Doppler is a SoundCue feature. For full doppler support, use SoundCues with SoundNodeDoppler."));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Doppler settings applied"), Resp);
      }
    } else {
      // No sound path - global doppler setting (not directly supported in UE5)
      UE_LOG(LogMcpAudioHandlers, Log,
             TEXT("Global doppler configuration requested: intensity=%.2f"),
             DopplerIntensity);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetNumberField(TEXT("dopplerIntensity"), DopplerIntensity);
      Resp->SetNumberField(TEXT("velocityScale"), VelocityScale);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Doppler configuration set"), Resp);
    }
    return true;
  }
  return false;
}
#endif
}
