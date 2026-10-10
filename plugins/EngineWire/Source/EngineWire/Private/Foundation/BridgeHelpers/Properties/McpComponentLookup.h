#pragma once

#include "CoreMinimal.h"

#include "ComponentReregisterContext.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
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
  // An editor world never ticks animation, so a skeletal mesh kept the pose it was initialised with: a new
  // AnimToPlay or SavedPosition showed only after the level reloaded. Re-initialising evaluates it once.
  USkeletalMeshComponent *Skeletal = Cast<USkeletalMeshComponent>(Component);
  if (Skeletal && Skeletal->GetWorld() && Skeletal->GetWorld()->WorldType == EWorldType::Editor) {
    Skeletal->InitAnim(true);
  }
}

/**
 * Collision lives inside BodyInstance behind setters. Writing CollisionProfileName
 * or CollisionEnabled raw skips the profile bookkeeping (a named profile
 * re-applies on load and puts the old value back) and the physics refresh, so
 * both the bare and the BodyInstance. spelling go through the component's own
 * setters, on live components and templates alike.
 */
static inline bool McpIsCollisionSetterKey(const UActorComponent *Component, const FString &Name) {
  auto Is = [&Name](const TCHAR *Field) {
    return Name.Equals(Field, ESearchCase::IgnoreCase) ||
           Name.Equals(FString(TEXT("BodyInstance.")) + Field, ESearchCase::IgnoreCase);
  };
  return Cast<UPrimitiveComponent>(Component) && (Is(TEXT("CollisionProfileName")) || Is(TEXT("CollisionEnabled")));
}

/** Applies a key McpIsCollisionSetterKey accepted; false with OutError on a bad value. */
static inline bool McpApplyCollisionSetterKey(UActorComponent *Component, const FString &Name,
                                              const TSharedPtr<FJsonValue> &Value, FString &OutError) {
  UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Component);
  if (!Primitive || !Value.IsValid() || Value->Type != EJson::String) {
    OutError = TEXT("expects a string (a collision profile or ECollisionEnabled name)");
    return false;
  }
  const FString Text = Value->AsString();
  if (Name.EndsWith(TEXT("CollisionProfileName"), ESearchCase::IgnoreCase)) {
    Primitive->SetCollisionProfileName(FName(*Text));
    return true;
  }
  const int64 Enabled = StaticEnum<ECollisionEnabled::Type>()->GetValueByNameString(Text);
  if (Enabled == INDEX_NONE) {
    OutError = FString::Printf(TEXT("'%s' is not an ECollisionEnabled value (NoCollision, QueryOnly, "
                                    "PhysicsOnly, QueryAndPhysics)"), *Text);
    return false;
  }
  Primitive->SetCollisionEnabled(static_cast<ECollisionEnabled::Type>(Enabled));
  // A static mesh on its mesh's default collision (a StaticMeshActor's) takes that profile back when it next
  // registers, as SetCollisionProfileName and the details panel's Custom preset both prevent.
  if (UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Primitive)) {
    Mesh->bUseDefaultCollision = false;
  }
  return true;
}

/**
 * A water body hands buoyancy over in its begin-overlap. An actor placed already in the water gets it at level load
 * only when it asks for overlaps during level streaming, and never when its root makes no overlap events: a
 * StaticMeshActor's mesh has them off, so a floater built on one sank with no warning. True when anything changed.
 */
static inline bool McpEnableWaterOverlaps(AActor *Actor) {
  UPrimitiveComponent *Root = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
  if (!Actor || (Actor->bGenerateOverlapEventsDuringLevelStreaming && (!Root || Root->GetGenerateOverlapEvents()))) {
    return false;
  }
  Actor->Modify();
  Actor->bGenerateOverlapEventsDuringLevelStreaming = true;
  if (Root) {
    Root->Modify();
    Root->SetGenerateOverlapEvents(true);
  }
  return true;
}
