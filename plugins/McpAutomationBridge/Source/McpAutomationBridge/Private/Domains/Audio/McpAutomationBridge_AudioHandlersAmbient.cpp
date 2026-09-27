#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
bool HandleAmbientActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("create_ambient_sound")) {
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

    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);

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
      Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"),
                          TEXT("NO_WORLD"));
      return true;
    }

    UAudioComponent *AudioComp = nullptr;
    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags = RF_Transactional;
    AAmbientSound* AmbientActor = World->SpawnActor<AAmbientSound>(AAmbientSound::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
    if (AmbientActor)
    {
      FString AmbientName; // dogfood #114: allow naming the spawned actor
      if (Payload->TryGetStringField(TEXT("name"), AmbientName) && !AmbientName.IsEmpty()) {
        AmbientActor->SetActorLabel(AmbientName);
      }
      AudioComp = AmbientActor->GetAudioComponent();
      if (AudioComp)
      {
        AudioComp->SetSound(Sound);
        AudioComp->bAutoActivate = false;
      }
    }
    if (!AudioComp)
    {
      AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, FRotator::ZeroRotator, FString());
    }

    if (AudioComp) {
      ApplyAudioComponentOptions(AudioComp, Payload);
      // Both were loaded and then dropped: the component never used the requested settings.
      if (Attenuation)
        AudioComp->AttenuationSettings = Attenuation;
      if (Concurrency)
        AudioComp->ConcurrencySet.Add(Concurrency);
      AudioComp->Activate(true);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      if (AmbientActor) { Resp->SetStringField(TEXT("actorName"), McpActorRef(AmbientActor)); } // contract output
      McpHandlerUtils::AddVerification(Resp, Sound);
      AddComponentVerification(Resp, AudioComp);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Ambient sound created"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create ambient sound"),
                          TEXT("SPAWN_FAILED"));
    }
    return true;
  }

  else if (Lower == TEXT("spawn_sound_at_location")) {
    // Similar to create_ambient_sound but explicit action name
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

    // Either vector spelling; rotation used to be read as an array only.
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

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

    // Honour name/actorName for the spawned sound actor (dogfood #114).
    FString SpawnedName;
    if (!Payload->TryGetStringField(TEXT("name"), SpawnedName)) {
      Payload->TryGetStringField(TEXT("actorName"), SpawnedName);
    }
    if (SpawnedName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("componentName"), SpawnedName); // dogfood #114
    }
    UAudioComponent *AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, SpawnedName);

    if (AudioComp) {
      ApplyAudioComponentOptions(AudioComp, Payload);
      AudioComp->Activate(true);
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      Resp->SetStringField(TEXT("componentPath"), AudioComp->GetPathName());
      McpHandlerUtils::AddVerification(Resp, Sound);
      AddComponentVerification(Resp, AudioComp);
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Sound spawned"), Resp);
    } else {
      Self->SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to spawn sound"), TEXT("SPAWN_FAILED"));
    }
    return true;
  }
  return false;
}
}
