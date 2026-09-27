#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"
#include "Misc/PackageName.h"

namespace McpAudioHandlers
{
bool BuildSanitizedAssetPath(
    const FString& InDirectory, const FString& AssetName,
    FString& OutDirectory, FString& OutFullPath)
{
  // Reject empty or invalid UObject names
  if (AssetName.IsEmpty()) return false;
  if (!FName::IsValidXName(AssetName, INVALID_OBJECTNAME_CHARACTERS)) {
    return false;
  }

  FString Directory = InDirectory.TrimStartAndEnd();
  if (!Directory.IsEmpty() && !Directory.StartsWith(TEXT("/"))) {
    Directory = TEXT("/Game/") + Directory;
  }

  OutDirectory = SanitizeProjectRelativePath(Directory);
  if (OutDirectory.IsEmpty()) return false;
  OutFullPath = SanitizeProjectRelativePath(
      FString::Printf(TEXT("%s/%s"), *OutDirectory, *AssetName));
  return !OutFullPath.IsEmpty();
}

/**
 * Finds an actor by object path/name or by actor label/name within an optional world.
 *
 * Searches first for an exact object path or registered name, and if not found and a World is provided,
 * iterates actors in that World comparing actor label and actor name case-insensitively.
 *
 * @param ActorName Actor object path, registered name, or actor label to search for.
 * @param World Optional world to search actor labels/names in when direct lookup fails.
 * @return `AActor*` Pointer to the matched actor, `nullptr` if no matching actor is found or ActorName is empty.
 */
AActor *FindAudioActorByName(const FString &ActorName, UWorld *World) {
  if (ActorName.IsEmpty())
    return nullptr;

  // Fast path: Direct object path/name
  AActor *Actor = FindObject<AActor>(nullptr, *ActorName);
  if (Actor && Actor->IsValidLowLevel())
    return Actor;

  return FindActorByNameInWorldForMcp(World, ActorName, true);
}

USceneComponent* EnsureAudioAttachRoot(AActor* Actor)
{
  if (!Actor)
    return nullptr;

  if (USceneComponent* Root = Actor->GetRootComponent())
    return Root;

  USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("McpAudioRoot"), RF_Transactional);
  if (!Root)
    return nullptr;

  Root->SetupAttachment(nullptr);
  Actor->SetRootComponent(Root);
  Actor->AddInstanceComponent(Root);
  Root->RegisterComponent();
  return Root;
}

UAudioComponent* CreateRegisteredAudioComponent(AActor* Owner, USoundBase* Sound, const FVector& Location, const FRotator& Rotation)
{
  if (!Owner || !Sound)
    return nullptr;

  UAudioComponent* AudioComp = NewObject<UAudioComponent>(Owner, NAME_None, RF_Transactional);
  if (!AudioComp)
    return nullptr;

  AudioComp->SetSound(Sound);
  AudioComp->bAutoActivate = false;
  AudioComp->SetRelativeLocation(Location);
  AudioComp->SetRelativeRotation(Rotation);
  Owner->AddInstanceComponent(AudioComp);

  if (USceneComponent* Root = EnsureAudioAttachRoot(Owner))
  {
    AudioComp->SetupAttachment(Root);
  }

  AudioComp->RegisterComponent();
  return AudioComp;
}

UAudioComponent* CreateAudioComponentAtEditorLocation(UWorld* World, USoundBase* Sound, const FVector& Location, const FRotator& Rotation, const FString& ActorName)
{
  if (!World || !Sound)
    return nullptr;

  FActorSpawnParameters SpawnParams;
  SpawnParams.Name = ActorName.IsEmpty() ? NAME_None : FName(*ActorName);
  SpawnParams.ObjectFlags = RF_Transactional;
  AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
  if (!Owner)
    return nullptr;

  if (!ActorName.IsEmpty())
    Owner->SetActorLabel(ActorName);

  UAudioComponent* AudioComp = CreateRegisteredAudioComponent(Owner, Sound, FVector::ZeroVector, FRotator::ZeroRotator);
  // A bare AActor has no root when it spawns, so the spawn transform was dropped and the
  // sound sat at the origin; the root exists now, so place the actor.
  Owner->SetActorLocationAndRotation(Location, Rotation);
  return AudioComp;
}

void ApplyAudioComponentOptions(UAudioComponent* AudioComp, const TSharedPtr<FJsonObject>& Payload)
{
  // Honour the requested componentName so later calls can address it (dogfood #112).
  FString RequestedName;
  if (Payload->TryGetStringField(TEXT("componentName"), RequestedName) && !RequestedName.IsEmpty() &&
      !FindObject<UObject>(AudioComp->GetOuter(), *RequestedName)) {
    AudioComp->Rename(*RequestedName, nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
  }
  double Volume = 1.0;
  if (Payload->TryGetNumberField(TEXT("volume"), Volume))
    AudioComp->SetVolumeMultiplier(static_cast<float>(Volume));
  double Pitch = 1.0;
  if (Payload->TryGetNumberField(TEXT("pitch"), Pitch))
    AudioComp->SetPitchMultiplier(static_cast<float>(Pitch));
}

FString AudioObjectPath(const FString& Path)
{
  return Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
}

bool LoadOptionalAudioSettings(const TSharedPtr<FJsonObject>& Payload, USoundAttenuation*& OutAttenuation, USoundConcurrency*& OutConcurrency, FString& OutError)
{
  const FString AttenPath = GetJsonStringField(Payload, TEXT("attenuationPath"));
  const FString ConcPath = GetJsonStringField(Payload, TEXT("concurrencyPath"));
  OutAttenuation = AttenPath.IsEmpty() ? nullptr : LoadObject<USoundAttenuation>(nullptr, *AudioObjectPath(AttenPath), nullptr, LOAD_NoWarn);
  OutConcurrency = ConcPath.IsEmpty() ? nullptr : LoadObject<USoundConcurrency>(nullptr, *AudioObjectPath(ConcPath), nullptr, LOAD_NoWarn);
  if (!AttenPath.IsEmpty() && !OutAttenuation)
  {
    OutError = FString::Printf(TEXT("SoundAttenuation not found: %s"), *AttenPath);
    return false;
  }
  if (!ConcPath.IsEmpty() && !OutConcurrency)
  {
    OutError = FString::Printf(TEXT("SoundConcurrency not found: %s"), *ConcPath);
    return false;
  }
  return true;
}

}
