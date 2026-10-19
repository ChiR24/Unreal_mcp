#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAudioPrivate.h"

namespace McpAudioHandlers
{
bool HandleComponentActions(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  // -------------------------------------------------------------------------
  // prime_sound
  // -------------------------------------------------------------------------
  // Pre-loads a sound asset for instant playback (reduces first-play latency).
  //
  // Payload:  { "soundPath": string }
  // Response: { "success": bool, "message": "Sound primed" }
  // -------------------------------------------------------------------------
  if (Lower == TEXT("prime_sound")) {
    FString SoundPath;
    Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound not found"),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    UGameplayStatics::PrimeSound(Sound);
    TSharedPtr<FJsonObject> Primed = McpHandlerUtils::CreateResultObject();
    Primed->SetBoolField(TEXT("success"), true);
    McpHandlerUtils::MarkNoAssetsChanged(Primed); // priming loads the sound, it changes no asset
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Sound primed"), Primed);
    return true;
  }

  // -------------------------------------------------------------------------
  // create_audio_component
  // -------------------------------------------------------------------------
  // Creates a UAudioComponent, optionally attached to an actor or at a location.
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "actorName"?: string, "componentName"?: string,
  //             "volume"?: number, "pitch"?: number, "autoPlay"?: bool }
  // Response: { "success": bool, "componentPath": string,
  //             "componentName": string }
  // -------------------------------------------------------------------------
  if (Lower.StartsWith(TEXT("create_audio_component"))) {
    FString SoundPath;
    if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath))
      Payload->TryGetStringField(TEXT("path"), SoundPath);
    if (SoundPath.IsEmpty()) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    USoundBase *Sound = ResolveSoundAsset(SoundPath);
    if (!Sound) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    FVector Location =
        ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation =
        ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    // actorName is the contract's spelling; the older attachTo stays as a fallback.
    const FString AttachTo = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("attachTo")});

    UAudioComponent *AudioComp = nullptr;
    UWorld *World =
        GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (!World) {
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"),
                          TEXT("NO_WORLD"));
      return true;
    }

    if (!AttachTo.IsEmpty()) {
      AActor *ParentActor = FindAudioActorByName(AttachTo, World);
      if (!ParentActor) {
        // A wrong name used to spawn a stray actor at the location and still report success.
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Actor '%s' not found; omit actorName to create the component on a new actor"), *AttachTo),
            TEXT("ACTOR_NOT_FOUND"));
        return true;
      }
      AudioComp = CreateRegisteredAudioComponent(ParentActor, Sound, Location, Rotation);
    } else {
      AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, FString());
    }

    if (AudioComp) {
      // volume and pitch are numbers in the contract; they were read as strings and so never applied.
      ApplyAudioComponentOptions(AudioComp, Payload);
      // autoPlay also decides whether the saved component starts with the game (bIsActive is transient).
      AudioComp->bAutoActivate = GetJsonBoolField(Payload, TEXT("autoPlay"), true);
      if (AudioComp->bAutoActivate)
        AudioComp->Activate(true);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      // The actor it is on, which without actorName is a new one: the reply named only the component.
      if (AActor *Owner = AudioComp->GetOwner()) {
        Resp->SetStringField(TEXT("actorName"), McpActorRef(Owner));
      }
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Audio component created"), Resp, FString());
      return true;
    }
    Self->SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create audio component"),
                        TEXT("CREATE_FAILED"));
    return true;
  }
  return false;
}
}
