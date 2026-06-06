#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

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

  // -------------------------------------------------------------------------
  // spawn_sound_at_location / audio_spawn_sound_at_location
  // -------------------------------------------------------------------------
  // Spawns a UAudioComponent at a world location (similar to create_ambient_sound
  // but with explicit action name and rotation support).
  //
  // Payload:  { "soundPath": string, "location"?: [x,y,z], "rotation"?: [p,y,r],
  //             "volume"?: number, "pitch"?: number, "startTime"?: number }
  // Response: { "componentName": string, "componentPath": string }
  // -------------------------------------------------------------------------
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
    double StartTime = 0.0;
    Payload->TryGetNumberField(TEXT("startTime"), StartTime);

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

    UAudioComponent *AudioComp = CreateAudioComponentAtEditorLocation(World, Sound, Location, Rotation, FString());

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
