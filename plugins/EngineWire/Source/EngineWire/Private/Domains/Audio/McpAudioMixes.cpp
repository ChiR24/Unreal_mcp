#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAudioPrivate.h"

namespace McpAudioHandlers
{
bool HandleMixActions(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  const bool bPush = Lower == TEXT("push_sound_mix");
  const bool bPop = Lower == TEXT("pop_sound_mix");
  const bool bBase = Lower == TEXT("set_base_sound_mix");
  const bool bOverride = Lower == TEXT("set_sound_mix_class_override");
  const bool bClear = Lower == TEXT("clear_sound_mix_class_override");
  if (!bPush && !bPop && !bBase && !bOverride && !bClear) {
    return false;
  }

  FString MixName;
  for (const TCHAR *Field : {TEXT("mixName"), TEXT("mix"), TEXT("name")}) {
    if (MixName.IsEmpty()) Payload->TryGetStringField(Field, MixName);
  }
  if (MixName.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  USoundMix *Mix = ResolveSoundMix(MixName);
  if (!Mix) {
    Self->SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Sound Mix not found: %s"), *MixName), TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  FString ClassName;
  Payload->TryGetStringField(TEXT("soundClassName"), ClassName);
  USoundClass *Class = (bOverride || bClear) ? ResolveSoundClass(ClassName) : nullptr;
  if ((bOverride || bClear) && !Class) {
    Self->SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Sound Class not found: %s"), *ClassName), TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  // Every mix action went to the editor world, muted while a game plays, so ducking a running game did nothing.
  UWorld *World = McpAudibleWorld();
  if (!World) {
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"), TEXT("NO_WORLD"));
    return true;
  }

  FString Done;
  if (bPush) {
    UGameplayStatics::PushSoundMixModifier(World, Mix);
    Done = TEXT("Sound Mix pushed");
  } else if (bPop) {
    UGameplayStatics::PopSoundMixModifier(World, Mix);
    Done = TEXT("Sound Mix popped");
  } else if (bBase) {
    UGameplayStatics::SetBaseSoundMix(World, Mix);
    Done = TEXT("Base Sound Mix set");
  } else if (bOverride) {
    double Volume = 1.0, Pitch = 1.0, FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    // The contract names the fade `fadeTime`; only `fadeInTime` was read, so it never arrived.
    if (!Payload->TryGetNumberField(TEXT("fadeTime"), FadeTime)) Payload->TryGetNumberField(TEXT("fadeInTime"), FadeTime);
    UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, (float)Volume, (float)Pitch, (float)FadeTime, true);
    Done = TEXT("Sound Mix class override set");
  } else {
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeOutTime"), FadeTime);
    UGameplayStatics::ClearSoundMixClassOverride(World, Mix, Class, (float)FadeTime);
    Done = TEXT("Sound Mix class override cleared");
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("inGame"), World->IsPlayInEditor());
  McpHandlerUtils::AddVerification(Resp, Mix);
  McpHandlerUtils::MarkNoAssetsChanged(Resp); // runtime mix state; the Sound Mix asset is used, not changed
  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
      Done + (World->IsPlayInEditor() ? TEXT(" on the running game") : TEXT(" in the editor")), Resp);
  return true;
}
}
