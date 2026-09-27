#pragma once

#include "CoreMinimal.h"

#include "ComponentReregisterContext.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

static inline UActorComponent *
FindComponentByName(AActor *Actor, const FString &ComponentName) {
  if (!Actor || ComponentName.IsEmpty()) {
    return nullptr;
  }

  const ESearchCase::Type Ci = ESearchCase::IgnoreCase;
  UActorComponent *ContainsMatch = nullptr;
  UActorComponent *StartsWithMatch = nullptr;

  TArray<UActorComponent *> Components;
  Actor->GetComponents(Components);

  for (UActorComponent *Component : Components) {
    if (!Component) {
      continue;
    }
    const FString Name = Component->GetName();
    const FString Path = Component->GetPathName();
    // Exact name, exact path, or a path ending in ".Needle" / ":Needle".
    const int32 Sep = Path.Len() - ComponentName.Len() - 1;
    if (Name.Equals(ComponentName, Ci) || Path.Equals(ComponentName, Ci) ||
        (Sep >= 0 && Path.EndsWith(ComponentName, Ci) &&
         (Path[Sep] == TEXT('.') || Path[Sep] == TEXT(':')))) {
      return Component;
    }
    if (!StartsWithMatch && Name.StartsWith(ComponentName, Ci)) {
      StartsWithMatch = Component;
    }
    if (!ContainsMatch && Path.Contains(ComponentName, Ci)) {
      ContainsMatch = Component;
    }
  }

  return StartsWithMatch ? StartsWithMatch : ContainsMatch;
}

/**
 * A property written straight into a live component skips the setters that
 * refresh what is drawn: bVisible=false read back false while the mesh kept
 * rendering. Whether a component renders at all is decided at registration, and
 * MarkRenderStateDirty() is a no-op while no render state exists, so re-register
 * in that case (it re-runs the engine's own check) and otherwise rebuild the
 * render state. Unregistered objects (Blueprint templates) have nothing to draw.
 */
static inline void McpRefreshComponentAfterEdit(UActorComponent *Component) {
  if (!Component || !Component->IsRegistered()) {
    return;
  }
  if (!Component->IsRenderStateCreated()) {
    FComponentReregisterContext ReregisterContext(Component);
  } else {
    Component->MarkRenderStateDirty();
  }
  if (USceneComponent *SceneComponent = Cast<USceneComponent>(Component)) {
    SceneComponent->UpdateComponentToWorld();
  }
}
