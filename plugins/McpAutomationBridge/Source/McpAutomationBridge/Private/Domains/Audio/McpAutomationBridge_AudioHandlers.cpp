#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

DEFINE_LOG_CATEGORY(LogMcpAudioHandlers);

bool UMcpAutomationBridgeSubsystem::HandleAudioAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  // Tool-name dispatch hands this handler Action == "manage_audio"; the real verb then
  // lives in the payload (dogfood #112: every runtime audio action fell through).
  FString Lower = Action.ToLower();
  if (Lower == TEXT("manage_audio") && Payload.IsValid())
  {
    FString SubAction;
    if (Payload->TryGetStringField(TEXT("subAction"), SubAction) || Payload->TryGetStringField(TEXT("action"), SubAction))
    {
      Lower = SubAction.ToLower();
    }
  }
  if (!Lower.StartsWith(TEXT("create_sound_")) &&
      !Lower.StartsWith(TEXT("play_sound_")) &&
      !Lower.StartsWith(TEXT("set_sound_")) &&
      !Lower.StartsWith(TEXT("push_sound_")) &&
      !Lower.StartsWith(TEXT("pop_sound_")) &&
      !Lower.StartsWith(TEXT("create_audio_")) &&
      !Lower.StartsWith(TEXT("create_ambient_")) &&
      !Lower.StartsWith(TEXT("create_reverb_")) &&
      !Lower.StartsWith(TEXT("fade_sound")) &&
      !Lower.StartsWith(TEXT("set_audio_")) &&
      !Lower.StartsWith(TEXT("clear_sound_")) &&
      !Lower.StartsWith(TEXT("set_base_sound_")) &&
      !Lower.StartsWith(TEXT("prime_")) &&
      !Lower.StartsWith(TEXT("spawn_sound_")) &&
      Lower != TEXT("stop_sound"))
  {
    return false;
  }

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
      &McpAudioHandlers::HandlePlaybackActions,
      &McpAudioHandlers::HandleAmbientActions,
      &McpAudioHandlers::HandleMixActions,
      &McpAudioHandlers::HandleComponentActions,
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

  SendAutomationResponse(
      RequestingSocket,
      RequestId,
      false,
      FString::Printf(TEXT("Unsupported audio action '%s'"), *Action),
      nullptr,
      TEXT("UNKNOWN_ACTION"));
  return true;
}
