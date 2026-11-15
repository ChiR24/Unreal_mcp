#pragma once

#include "CoreMinimal.h"

#include <type_traits>

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Editor.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "GameFramework/Actor.h"

// The undo step of a spawn, "Spawn Actors": a spawn ran outside any transaction, so undo answered
// NOTHING_TO_UNDO and the actors stayed. A single spawn opens it before it touches the level and
// describes it in its reply; a run inside spawn_batch (captured) gets none, because the batch holds
// one for every item.
inline TUniquePtr<FMcpScopedEditorTransaction> McpBeginSpawnTransaction(const FString &RequestId) {
  if (FMcpResponseCaptureRegistry::Get().IsBatchStep(RequestId)) {
    return nullptr;
  }
  return MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(TEXT("Spawn Actors")),
                                                  EMcpMutationDurability::EditorStateOnly, TArray<UObject *>());
}

// Spawn directly into TargetWorld to avoid viewport placement paths that are unsafe under null-renderer automation; in
// the editor world the actor goes into its current level, which records the add for undo.
template <typename T = AActor>
static inline T *
McpSpawnActorInWorld(UWorld *TargetWorld, UClass *ActorClass, const FVector &Location,
                     const FRotator &Rotation, const FString &OptionalLabel) {
  static_assert(std::is_base_of<AActor, T>::value,
                "T must be derived from AActor");

  if (!GEditor || !ActorClass || !TargetWorld)
    return nullptr;

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (TargetWorld != GEditor->PlayWorld) {
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

// The running game while Play runs, else the editor level.
template <typename T = AActor>
static inline T *SpawnActorInActiveWorld(UClass *ActorClass, const FVector &Location, const FRotator &Rotation,
                                          const FString &OptionalLabel = FString()) {
  return McpSpawnActorInWorld<T>(McpHandlerUtils::GetPlayOrEditorWorld(), ActorClass, Location, Rotation, OptionalLabel);
}

// The editor level, while Play runs too: an actor a configure call authors belongs to the level, not the running game.
template <typename T = AActor>
static inline T *SpawnActorInEditorWorld(UClass *ActorClass, const FVector &Location, const FRotator &Rotation,
                                          const FString &OptionalLabel = FString()) {
  return McpSpawnActorInWorld<T>(McpHandlerUtils::GetEditorWorld(), ActorClass, Location, Rotation, OptionalLabel);
}

// The editor level's actor of ActorClass (any class when null) labelled or named ActorName (any case), or with no
// ActorName the first one of the class; only actors in a level the request may touch. Null when there is none.
AActor *McpFindActorByNameOrClass(UClass *ActorClass, const FString &ActorName);

// McpFindActorByNameOrClass, else an actor of ActorClass spawned in the editor level at Location and labelled ActorName
// (the class name when empty); bOutSpawned says which. Null with no class, or when the spawn failed.
AActor *McpFindOrSpawnActor(UClass *ActorClass, const FString &ActorName, const FVector &Location,
                            const FRotator &Rotation, bool *bOutSpawned = nullptr);
