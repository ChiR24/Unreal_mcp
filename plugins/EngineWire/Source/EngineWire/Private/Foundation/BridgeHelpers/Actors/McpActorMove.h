#pragma once

#include "CoreMinimal.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// An actor moved from code in the editor world kept its collision queries on the old spot: a trace,
// a foliage drop or a placement check missed it where it now stands, though PIE (a fresh copy) was
// right. The gizmo and the Details panel end every move with PostEditMove (construction scripts,
// navigation, listeners); a move made here ends the same way, and its bodies are rebuilt where it is.
static inline void McpFinishEditorMove(AActor *Actor) {
  UWorld *World = Actor ? Actor->GetWorld() : nullptr;
  if (!World || World->IsGameWorld())
    return;
  Actor->PostEditMove(true);
  TArray<UPrimitiveComponent *> Primitives;
  Actor->GetComponents(Primitives);
  for (UPrimitiveComponent *Primitive : Primitives) {
    if (Primitive->IsPhysicsStateCreated())
      Primitive->RecreatePhysicsState();
  }
}
