#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
// The 2D sounds play_sound started, for stop_sound, pause_sound and resume_sound.
TArray<TWeakObjectPtr<UAudioComponent>> &McpPreviewSounds()
{
  static TArray<TWeakObjectPtr<UAudioComponent>> Sounds;
  return Sounds;
}

bool HandlePlaybackActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("play_sound_at_location")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Both {x,y,z} and [x,y,z] (and the rotator forms): the array-only read ignored
    // every object the gateway passes, so the sound always played at the origin.
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    USoundConcurrency *Concurrency = nullptr;
    FString LoadError;
    if (!LoadOptionalAudioSettings(Payload, Attenuation, Concurrency, LoadError)) {
      Self->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (!GEditor)
    {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("No world context available"), TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySoundAtLocation(
        World, Sound, Location, Rotation, (float)Volume, (float)Pitch,
        (float)StartTime, Attenuation, Concurrency);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    Resp->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Location));
    McpHandlerUtils::MarkNoAssetsChanged(Resp); // playing a sound changes no asset

    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played at location"), Resp);
    return true;
  }

  // Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
  //             "startTime"?: number }
  // Response: { "success": bool, "soundPath": string, "volume": number,
  //             "pitch": number }
  else if (Lower == TEXT("play_sound_2d")) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) ||
        SoundPath.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Sound asset not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    if (!GEditor) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    // A component, not a fire-and-forget PlaySound2D, so stop_sound can stop it:
    // a looping preview (the stage music) could not be stopped at all.
    if (UAudioComponent *Preview = UGameplayStatics::SpawnSound2D(
            World, Sound, (float)Volume, (float)Pitch, (float)StartTime)) {
      McpPreviewSounds().Add(Preview);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    Resp->SetNumberField(TEXT("volume"), Volume);
    Resp->SetNumberField(TEXT("pitch"), Pitch);

    // Sound played - add sound asset verification. The asset is used, not changed.
    McpHandlerUtils::AddVerification(Resp, Sound);
    McpHandlerUtils::MarkNoAssetsChanged(Resp);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played 2D"), Resp);
    return true;
  }

  // Payload: { "soundPath"?: string, "all"?: bool }. Stops the 2D sounds
  // play_sound started (only that sound's when soundPath is given); all also
  // stops every sound the editor and a running game play, the game's own music too.
  else if (Lower == TEXT("stop_sound")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    bool bAll = false;
    Payload->TryGetBoolField(TEXT("all"), bAll);
    USoundBase *Only = SoundPath.IsEmpty() ? nullptr : ResolveSoundAsset(SoundPath);
    if (!SoundPath.IsEmpty() && !Only) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"),
                                TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    int32 Stopped = 0;
    for (const TWeakObjectPtr<UAudioComponent> &Weak : McpPreviewSounds()) {
      UAudioComponent *Preview = Weak.Get();
      if (Preview && Preview->IsPlaying() && (!Only || Preview->Sound == Only)) {
        Preview->Stop();
        ++Stopped;
      }
    }
    McpPreviewSounds().RemoveAll([](const TWeakObjectPtr<UAudioComponent> &Weak) {
      return !Weak.IsValid() || !Weak->IsPlaying();
    });
    int32 Devices = 0;
    if (bAll && GEngine) {
      for (const FWorldContext &Context : GEngine->GetWorldContexts()) {
        FAudioDeviceHandle Device = Context.World() ? Context.World()->GetAudioDevice() : FAudioDeviceHandle();
        if (Device.IsValid()) {
          Device->StopAllSounds(true);
          ++Devices;
        }
      }
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("stopped"), Stopped);
    Resp->SetBoolField(TEXT("allStopped"), Devices > 0);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
        bAll ? TEXT("Every sound stopped") : FString::Printf(TEXT("Stopped %d sound(s) play_sound started"), Stopped), Resp);
    return true;
  }

  // Payload:  { "soundPath": string, "actorName": string, "attachPointName"?: string,
  //             "componentName"?: string, "volume"?: number, "pitch"?: number }
  // Response: { "componentName": string, "attachedTo": string, "playing": bool }
  else if (Lower == TEXT("play_sound_attached")) {
    FString SoundPath, ActorName, AttachPoint;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Payload->TryGetStringField(TEXT("attachPointName"), AttachPoint);

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    if (!GEditor)
    {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor not available"), TEXT("NO_EDITOR"));
      return true;
    }
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    AActor *TargetActor = FindAudioActorByName(ActorName, World);
    if (!TargetActor) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"),
                          TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    // attachPointName names a scene component or a socket; a component name wins.
    USceneComponent *AttachComp = EnsureAudioAttachRoot(TargetActor);
    FName SocketName = NAME_None;
    if (!AttachPoint.IsEmpty()) {
      USceneComponent *FoundComp = nullptr;
      TArray<USceneComponent *> Components;
      TargetActor->GetComponents(Components);
      for (USceneComponent *Comp : Components) {
        if (!FoundComp && Comp && Comp->GetName() == AttachPoint)
          FoundComp = Comp;
      }
      for (USceneComponent *Comp : Components) {
        if (!FoundComp && Comp && Comp->DoesSocketExist(FName(*AttachPoint))) {
          FoundComp = Comp;
          SocketName = FName(*AttachPoint);
        }
      }
      if (!FoundComp) {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("'%s' is neither a component nor a socket on the actor"), *AttachPoint),
            TEXT("ATTACH_POINT_NOT_FOUND"));
        return true;
      }
      AttachComp = FoundComp;
    }

    // The attach point used to be resolved and then dropped (the component went on the
    // root, inactive), and nothing started it, yet the reply said "Sound attached".
    UAudioComponent *AudioComp = AttachComp
        ? CreateRegisteredAudioComponent(TargetActor, Sound, FVector::ZeroVector, FRotator::ZeroRotator)
        : nullptr;

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (AudioComp) {
      AudioComp->AttachToComponent(AttachComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
      ApplyAudioComponentOptions(AudioComp, Payload);
      AudioComp->Play();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      Resp->SetStringField(TEXT("attachedTo"), AttachComp->GetName());
      Resp->SetBoolField(TEXT("playing"), AudioComp->IsPlaying());
      McpHandlerUtils::AddVerification(Resp, Sound);
      McpHandlerUtils::MarkNoAssetsChanged(Resp); // the sound asset is used, not changed
      AddComponentVerification(Resp, AudioComp);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound attached"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to attach sound"),
                          TEXT("ATTACH_FAILED"));
    }
    return true;
  }
  return false;
}
}
