#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
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

  // Fallback: Label search (limited scope)
  if (World) {
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
          It->GetName().Equals(ActorName, ESearchCase::IgnoreCase)) {
        return *It;
      }
    }
  }
  return nullptr;
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

#if WITH_EDITOR
  if (!ActorName.IsEmpty())
    Owner->SetActorLabel(ActorName);
#endif

  return CreateRegisteredAudioComponent(Owner, Sound, FVector::ZeroVector, FRotator::ZeroRotator);
}

#endif
}
