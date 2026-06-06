#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandlePlaybackActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("play_sound_at_location") ||
             Lower == TEXT("audio_play_sound_at_location")) {
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

    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    }
    const TArray<TSharedPtr<FJsonValue>> *RotArr;
    if (Payload->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr &&
        RotArr->Num() >= 3) {
      Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(),
                          (*RotArr)[2]->AsNumber());
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

    USoundAttenuation *Attenuation = nullptr;
    FString AttenPath;
    if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenPath) &&
        !AttenPath.IsEmpty()) {
      Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenPath);
    }

    USoundConcurrency *Concurrency = nullptr;
    FString ConcPath;
    if (Payload->TryGetStringField(TEXT("concurrencyPath"), ConcPath) &&
        !ConcPath.IsEmpty()) {
      Concurrency = LoadObject<USoundConcurrency>(nullptr, *ConcPath);
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
    TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
    LocObj->SetNumberField(TEXT("x"), Location.X);
    LocObj->SetNumberField(TEXT("y"), Location.Y);
    LocObj->SetNumberField(TEXT("z"), Location.Z);
    Resp->SetObjectField(TEXT("location"), LocObj);

    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played at location"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // play_sound_2d / audio_play_sound_2d
  // -------------------------------------------------------------------------
  // Plays a non-spatialized 2D sound with optional volume, pitch, start time.
  //
  // Payload:  { "soundPath": string, "volume"?: number, "pitch"?: number,
  //             "startTime"?: number }
  // Response: { "success": bool, "soundPath": string, "volume": number,
  //             "pitch": number }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("play_sound_2d") ||
             Lower == TEXT("audio_play_sound_2d")) {
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

    if (!GEditor)
      return true;
    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UGameplayStatics::PlaySound2D(World, Sound, (float)Volume, (float)Pitch,
                                  (float)StartTime);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("soundPath"), SoundPath);
    Resp->SetNumberField(TEXT("volume"), Volume);
    Resp->SetNumberField(TEXT("pitch"), Pitch);

    // Sound played - add sound asset verification
    McpHandlerUtils::AddVerification(Resp, Sound);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound played 2D"), Resp);
    return true;
  }

  // -------------------------------------------------------------------------
  // play_sound_attached / audio_play_sound_attached
  // -------------------------------------------------------------------------
  // Attaches and plays a sound on an actor's component or socket.
  //
  // Payload:  { "soundPath": string, "actorName": string,
  //             "attachPointName"?: string }
  // Response: { "componentName": string }
  // -------------------------------------------------------------------------
  else if (Lower == TEXT("play_sound_attached") ||
             Lower == TEXT("audio_play_sound_attached")) {
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

    USceneComponent *AttachComp = EnsureAudioAttachRoot(TargetActor);
    if (!AttachPoint.IsEmpty()) {
      // Try to find socket or component
      USceneComponent *FoundComp = nullptr;
      TArray<USceneComponent *> Components;
      TargetActor->GetComponents(Components);
      for (USceneComponent *Comp : Components) {
        if (Comp->GetName() == AttachPoint ||
            Comp->DoesSocketExist(FName(*AttachPoint))) {
          FoundComp = Comp;
          break;
        }
      }
      if (FoundComp)
        AttachComp = FoundComp;
    }

    UAudioComponent *AudioComp = nullptr;
    if (AttachComp)
    {
      AudioComp = CreateRegisteredAudioComponent(TargetActor, Sound, FVector::ZeroVector, FRotator::ZeroRotator);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (AudioComp) {
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      McpHandlerUtils::AddVerification(Resp, Sound);
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
#endif
}
