#pragma once

#include "CoreMinimal.h"

#include <type_traits>

#if WITH_EDITOR
#include "Editor.h"
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

  UWorld *TargetWorld = GEditor->PlayWorld
                            ? GEditor->PlayWorld.Get()
                            : GEditor->GetEditorWorldContext().World();
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
#endif
