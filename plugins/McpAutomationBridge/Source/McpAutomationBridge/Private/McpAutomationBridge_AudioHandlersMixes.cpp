#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleMixActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("push_sound_mix") ||
             Lower == TEXT("audio_push_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PushSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        McpHandlerUtils::AddVerification(Resp, Mix);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix pushed"), Resp);
      } else {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // pop_sound_mix / audio_pop_sound_mix
  // -------------------------------------------------------------------------
  // Pops a SoundMix modifier from the audio stack.
  //
  // Payload:  { "mixName": string }
  // Response: { "success": bool, "mixName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("pop_sound_mix") ||
             Lower == TEXT("audio_pop_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) ||
        MixName.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    if (Mix) {
      if (GEditor && GEditor->GetEditorWorldContext().World()) {
        UGameplayStatics::PopSoundMixModifier(
            GEditor->GetEditorWorldContext().World(), Mix);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("mixName"), MixName);
        McpHandlerUtils::AddVerification(Resp, Mix);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("SoundMix popped"), Resp);
      } else {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("No World Context"), TEXT("NO_WORLD"));
      }
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_sound_mix_class_override / audio_set_sound_mix_class_override
  // -------------------------------------------------------------------------
  // Overrides a SoundClass's volume/pitch within a SoundMix.
  //
  // Payload:  { "mixName": string, "soundClassName": string,
  //             "volume"?: number, "pitch"?: number, "fadeInTime"?: number,
  //             "applyToChildren"?: bool }
  // Response: { "success": bool, "mixName": string, "className": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("set_sound_mix_class_override") ||
             Lower == TEXT("audio_set_sound_mix_class_override")) {
    FString MixName, ClassName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
      if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
        Payload->TryGetStringField(TEXT("name"), MixName);
      }
    }
    if (!Payload->TryGetStringField(TEXT("soundClassName"), ClassName) || ClassName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("soundClass"), ClassName);
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

    if (!Mix || !Class) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeInTime"), FadeTime);
    bool bApply = true;
    Payload->TryGetBoolField(TEXT("applyToChildren"), bApply);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class, (float)Volume,
          (float)Pitch, (float)FadeTime, bApply);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("mixName"), MixName);
      Resp->SetStringField(TEXT("className"), ClassName);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override set"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // clear_sound_mix_class_override / audio_clear_sound_mix_class_override
  // -------------------------------------------------------------------------
  // Clears a SoundClass override from a SoundMix with optional fade out.
  //
  // Payload:  { "mixName": string, "soundClassName": string,
  //             "fadeOutTime"?: number }
  // Response: { "success": bool, "message": "Sound mix override cleared" }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("clear_sound_mix_class_override") ||
             Lower == TEXT("audio_clear_sound_mix_class_override")) {
    FString MixName, ClassName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
      if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
        Payload->TryGetStringField(TEXT("name"), MixName);
      }
    }
    if (!Payload->TryGetStringField(TEXT("soundClassName"), ClassName) || ClassName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("soundClass"), ClassName);
    }

    USoundMix *Mix = ResolveSoundMix(MixName);
    USoundClass *Class = ResolveSoundClass(ClassName);

    if (!Mix || !Class) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Mix or Class not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double FadeTime = 1.0;
    Payload->TryGetNumberField(TEXT("fadeOutTime"), FadeTime);

    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::ClearSoundMixClassOverride(
          GEditor->GetEditorWorldContext().World(), Mix, Class,
          (float)FadeTime);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound mix override cleared"), nullptr);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }

  // -------------------------------------------------------------------------
  // set_base_sound_mix
  // -------------------------------------------------------------------------
  // Sets the base (default) SoundMix for the world.
  //
  // Payload:  { "mixName": string }
  // Response: { "success": bool, "message": "Base sound mix set" }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("set_base_sound_mix")) {
    FString MixName;
    if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty()) {
      if (!Payload->TryGetStringField(TEXT("mix"), MixName) || MixName.IsEmpty()) {
        Payload->TryGetStringField(TEXT("name"), MixName);
      }
    }
    USoundMix *Mix = ResolveSoundMix(MixName);
    if (!Mix) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Mix not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (GEditor && GEditor->GetEditorWorldContext().World()) {
      UGameplayStatics::SetBaseSoundMix(
          GEditor->GetEditorWorldContext().World(), Mix);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Base sound mix set"), nullptr);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
    }
    return true;
  }
  return false;
}
#endif
}
