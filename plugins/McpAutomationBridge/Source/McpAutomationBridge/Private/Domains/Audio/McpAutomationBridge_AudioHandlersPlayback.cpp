#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
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
