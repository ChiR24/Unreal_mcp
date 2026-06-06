#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR && ENGINE_MAJOR_VERSION >= 5
#include "Components/ActorComponent.h"
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
#endif
