#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
bool HandleAmbientActions(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower == TEXT("create_ambient_sound") ||
             Lower == TEXT("audio_create_ambient_sound")) {
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
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    } else {
      const TSharedPtr<FJsonObject> *LocObj = nullptr; // {x, y, z} spelling (dogfood #226)
      if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        Location = FVector((*LocObj)->GetNumberField(TEXT("x")), (*LocObj)->GetNumberField(TEXT("y")), (*LocObj)->GetNumberField(TEXT("z")));
      }
    }

    double Volume = 1.0;
    Payload->TryGetNumberField(TEXT("volume"), Volume);
    double Pitch = 1.0;
    Payload->TryGetNumberField(TEXT("pitch"), Pitch);

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
        AudioComp->SetVolumeMultiplier((float)Volume);
        AudioComp->SetPitchMultiplier((float)Pitch);
        AudioComp->bAutoActivate = false;
      }
    }
    if (!AudioComp)
    {
      AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, FRotator::ZeroRotator, FString());
    }

    if (AudioComp) {
      AudioComp->Activate(true);

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("componentName"), AudioComp->GetName());
      if (AmbientActor) { Resp->SetStringField(TEXT("actorName"), AmbientActor->GetActorLabel()); } // contract output
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

  else if (Lower == TEXT("spawn_sound_at_location") ||
             Lower == TEXT("audio_spawn_sound_at_location")) {
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

    FVector Location = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>> *LocArr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr &&
        LocArr->Num() >= 3) {
      Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(),
                         (*LocArr)[2]->AsNumber());
    } else {
      const TSharedPtr<FJsonObject> *LocObj = nullptr; // {x, y, z} spelling (dogfood #226)
      if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
        Location = FVector((*LocObj)->GetNumberField(TEXT("x")), (*LocObj)->GetNumberField(TEXT("y")), (*LocObj)->GetNumberField(TEXT("z")));
      }
    }

    FRotator Rotation = FRotator::ZeroRotator;
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
      AudioComp->SetVolumeMultiplier((float)Volume);
      AudioComp->SetPitchMultiplier((float)Pitch);
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
#endif
}
