#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
#include "ComponentReregisterContext.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

static inline UActorComponent *
FindComponentByName(AActor *Actor, const FString &ComponentName) {
  if (!Actor || ComponentName.IsEmpty()) {
    return nullptr;
  }

  const FString Needle = ComponentName.ToLower();
  UActorComponent *ContainsMatch = nullptr;
  UActorComponent *StartsWithMatch = nullptr;

  TArray<UActorComponent *> Components;
  Actor->GetComponents(Components);

  for (UActorComponent *Component : Components) {
    if (!Component) {
      continue;
    }

    const FString ComponentNameLower = Component->GetName().ToLower();
    const FString ComponentPath = Component->GetPathName().ToLower();

    if (ComponentNameLower.Equals(Needle) || ComponentPath.Equals(Needle) ||
        ComponentPath.EndsWith(
            FString::Printf(TEXT(".%s"), *Needle)) ||
        ComponentPath.EndsWith(FString::Printf(TEXT(":%s"), *Needle))) {
      return Component;
    }

    if (ComponentNameLower.StartsWith(Needle) && !StartsWithMatch) {
      StartsWithMatch = Component;
    }
    if (!ContainsMatch && ComponentPath.Contains(Needle)) {
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
#endif
