#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

DEFINE_LOG_CATEGORY(LogMcpAudioHandlers);

bool UMcpAutomationBridgeSubsystem::HandleAudioAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("audio_")) &&
      !Lower.StartsWith(TEXT("create_sound_")) &&
      !Lower.StartsWith(TEXT("play_sound_")) &&
      !Lower.StartsWith(TEXT("set_sound_")) &&
      !Lower.StartsWith(TEXT("push_sound_")) &&
      !Lower.StartsWith(TEXT("pop_sound_")) &&
      !Lower.StartsWith(TEXT("create_audio_")) &&
      !Lower.StartsWith(TEXT("create_ambient_")) &&
      !Lower.StartsWith(TEXT("create_reverb_")) &&
      !Lower.StartsWith(TEXT("enable_audio_")) &&
      !Lower.StartsWith(TEXT("fade_sound")) &&
      !Lower.StartsWith(TEXT("set_doppler_")) &&
      !Lower.StartsWith(TEXT("set_audio_")) &&
      !Lower.StartsWith(TEXT("clear_sound_")) &&
      !Lower.StartsWith(TEXT("set_base_sound_")) &&
      !Lower.StartsWith(TEXT("prime_")) &&
      !Lower.StartsWith(TEXT("spawn_sound_")) &&
      !Lower.Equals(TEXT("add_source_effect")))
  {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid())
  {
    SendAutomationError(
        RequestingSocket,
        RequestId,
        TEXT("Audio payload missing"),
        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  const McpAudioHandlers::FAudioActionHandler Handlers[] = {
      &McpAudioHandlers::HandleAssetActions,
      &McpAudioHandlers::HandlePlaybackActions,
      &McpAudioHandlers::HandleAmbientActions,
      &McpAudioHandlers::HandleMixActions,
      &McpAudioHandlers::HandleComponentActions,
      &McpAudioHandlers::HandleAnalysisActions,
      &McpAudioHandlers::HandleSpatialActions,
      &McpAudioHandlers::HandleFadeAndReverbActions,
  };
  for (McpAudioHandlers::FAudioActionHandler Handler : Handlers)
  {
    if (Handler(this, RequestId, Lower, Payload, RequestingSocket))
    {
      return true;
    }
  }

  if (Lower == TEXT("create_dialogue_voice"))
  {
    return HandleCreateDialogueVoice(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_dialogue_wave"))
  {
    return HandleCreateDialogueWave(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("set_dialogue_context"))
  {
    return HandleSetDialogueContext(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_reverb_effect"))
  {
    return HandleCreateReverbEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_source_effect_chain"))
  {
    return HandleCreateSourceEffectChain(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("add_source_effect"))
  {
    return HandleAddSourceEffect(RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("create_submix_effect"))
  {
    return HandleCreateSubmixEffect(RequestId, Payload, RequestingSocket);
  }

  SendAutomationResponse(
      RequestingSocket,
      RequestId,
      false,
      FString::Printf(TEXT("Unsupported audio action '%s'"), *Action),
      nullptr,
      TEXT("UNKNOWN_ACTION"));
  return true;
#else
  SendAutomationResponse(
      RequestingSocket,
      RequestId,
      false,
      TEXT("Audio actions require editor build"),
      nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
