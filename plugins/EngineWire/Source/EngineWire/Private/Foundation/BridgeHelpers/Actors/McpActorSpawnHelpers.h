#pragma once

#include "CoreMinimal.h"

#include <type_traits>

#include "Editor.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "GameFramework/Actor.h"

// Spawn directly into the active PIE/editor world to avoid viewport placement
// paths that are unsafe under null-renderer automation.
template <typename T = AActor>
static inline T *
SpawnActorInActiveWorld(UClass *ActorClass, const FVector &Location,
                        const FRotator &Rotation,
                        const FString &OptionalLabel = FString()) {
  static_assert(std::is_base_of<AActor, T>::value,
                "T must be derived from AActor");

  if (!GEditor || !ActorClass)
    return nullptr;

  UWorld *TargetWorld = McpHandlerUtils::GetPlayOrEditorWorld();
  if (!TargetWorld)
    return nullptr;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }

  AActor *Spawned =
      TargetWorld->SpawnActor(ActorClass, &Location, &Rotation, SpawnParams);
  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
  }

  if (Spawned && !OptionalLabel.IsEmpty()) {
    Spawned->SetActorLabel(OptionalLabel);
  }

  return Cast<T>(Spawned);
}

// The editor level's actor of ActorClass (any class when null) labelled or named ActorName (any case), or with no
// ActorName the first one of the class; only actors in a level the request may touch. Null when there is none.
AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName);

// McpFindActorByNameOrClass, else an actor of ActorClass spawned at Location and labelled ActorName (the class name
// when empty); bOutSpawned says which. Null with no class, or when the spawn failed.
AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                            const FRotator &Rotation, bool *bOutSpawned = nullptr);
