#pragma once

#include "CoreMinimal.h"

#include "ComponentReregisterContext.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

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
/** True when Name is a collision preset of the project; OutError lists the presets when it is not. */
static inline bool McpIsCollisionProfile(const FString &Name, FString &OutError) {
  FCollisionResponseTemplate Template;
  if (UCollisionProfile::Get()->GetProfileTemplate(FName(*Name), Template)) {
    return true;
  }
  TArray<TSharedPtr<FName>> Profiles;
  UCollisionProfile::GetProfileNames(Profiles);
  TArray<FString> Listed;
  for (const TSharedPtr<FName> &Profile : Profiles) {
    if (Profile.IsValid()) Listed.Add(Profile->ToString());
  }
  OutError = FString::Printf(TEXT("'%s' is not a collision preset; the project has: %s"), *Name,
                             *FString::Join(Listed, TEXT(", ")));
  return false;
}

/** The channels a project uses, as Project Settings names them: the engine's, and its own trace and object ones. */
static inline TArray<TPair<FString, ECollisionChannel>> McpCollisionChannels() {
  TArray<TPair<FString, ECollisionChannel>> Channels;
  for (int32 Index = 0; Index < ECC_MAX; ++Index) {
    const FString Name = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Index).ToString();
    // An unconfigured project channel keeps its slot name, and the engine's internal ones are hidden.
    if (Name != TEXT("None") && !Name.StartsWith(TEXT("GameTraceChannel")) && !Name.StartsWith(TEXT("EngineTraceChannel")) &&
        !Name.Contains(TEXT("_Deprecated")))
      Channels.Emplace(Name, static_cast<ECollisionChannel>(Index));
  }
  return Channels;
}

static inline const TCHAR *McpCollisionResponseName(ECollisionResponse Response) {
  return Response == ECR_Block ? TEXT("Block") : Response == ECR_Overlap ? TEXT("Overlap") : TEXT("Ignore");
}

/** {channel: Block | Overlap | Ignore} for every channel the project uses: BodyInstance keeps them out of reflection. */
static inline TSharedPtr<FJsonObject> McpCollisionResponsesOf(const UPrimitiveComponent *Primitive) {
  TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
  for (const TPair<FString, ECollisionChannel> &Channel : McpCollisionChannels()) {
    Out->SetStringField(Channel.Key, McpCollisionResponseName(Primitive->GetCollisionResponseToChannel(Channel.Value)));
  }
  return Out;
}

/**
 * Per-channel collision responses, {"Pawn": "Ignore", "Visibility": "Block"}: each channel named as Project Settings
 * names it, each response Block, Overlap or Ignore. Every entry is read before any applies, so a bad one changes
 * nothing; OutError names it and what would work.
 */
static inline bool McpReadCollisionResponses(const TSharedPtr<FJsonObject> &Responses,
                                             TArray<TPair<ECollisionChannel, ECollisionResponse>> &Out,
                                             FString &OutError) {
  const TArray<TPair<FString, ECollisionChannel>> Channels = McpCollisionChannels();
  for (const auto &Pair : Responses->Values) {
    const FString Channel(Pair.Key.Len(), *Pair.Key);
    const TPair<FString, ECollisionChannel> *Found = Channels.FindByPredicate(
        [&Channel](const TPair<FString, ECollisionChannel> &Entry) { return Entry.Key.Equals(Channel, ESearchCase::IgnoreCase); });
    const FString Value = Pair.Value.IsValid() && Pair.Value->Type == EJson::String ? Pair.Value->AsString() : FString();
    const ECollisionResponse Response = Value.Equals(TEXT("Block"), ESearchCase::IgnoreCase)     ? ECR_Block
                                        : Value.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase) ? ECR_Overlap
                                        : Value.Equals(TEXT("Ignore"), ESearchCase::IgnoreCase)  ? ECR_Ignore
                                                                                                 : ECR_MAX;
    if (!Found) {
      TArray<FString> Named;
      for (const TPair<FString, ECollisionChannel> &Entry : Channels) Named.Add(Entry.Key);
      OutError = FString::Printf(TEXT("'%s' is not a collision channel; the project has: %s"), *Channel,
                                 *FString::Join(Named, TEXT(", ")));
      return false;
    }
    if (Response == ECR_MAX) {
      OutError = FString::Printf(TEXT("%s: '%s' is not a response; use Block, Overlap or Ignore"), *Channel, *Value);
      return false;
    }
    Out.Emplace(Found->Value, Response);
  }
  return true;
}

/** Sets read responses; a static mesh then stops taking its mesh's default collision back when it registers. */
static inline void McpApplyCollisionResponses(UPrimitiveComponent *Primitive,
                                              const TArray<TPair<ECollisionChannel, ECollisionResponse>> &Responses) {
  for (const TPair<ECollisionChannel, ECollisionResponse> &Entry : Responses) {
    Primitive->SetCollisionResponseToChannel(Entry.Key, Entry.Value);
  }
  if (UStaticMeshComponent *Mesh = Cast<UStaticMeshComponent>(Primitive)) {
    Mesh->bUseDefaultCollision = false;
  }
}

static inline bool McpIsCollisionSetterKey(const UActorComponent *Component, const FString &Name) {
  auto Is = [&Name](const TCHAR *Field) {
    return Name.Equals(Field, ESearchCase::IgnoreCase) ||
           Name.Equals(FString(TEXT("BodyInstance.")) + Field, ESearchCase::IgnoreCase);
  };
  return Cast<UPrimitiveComponent>(Component) &&
         (Is(TEXT("CollisionProfileName")) || Is(TEXT("CollisionEnabled")) || Is(TEXT("CollisionResponses")) ||
          Is(TEXT("PhysMaterialOverride")));
}

/** Applies a key McpIsCollisionSetterKey accepted; false with OutError on a bad value. */
static inline bool McpApplyCollisionSetterKey(UActorComponent *Component, const FString &Name,
                                              const TSharedPtr<FJsonValue> &Value, FString &OutError) {
  UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Component);
  // A physical material path, or None or null to clear: the setter rebuilds the body's materials, a raw write does not.
  if (Primitive && Name.EndsWith(TEXT("PhysMaterialOverride"), ESearchCase::IgnoreCase)) {
    const FString Path = Value.IsValid() && Value->Type == EJson::String ? Value->AsString() : FString();
    UPhysicalMaterial *Material = nullptr;
    if (!Path.IsEmpty() && !Path.Equals(TEXT("None"), ESearchCase::IgnoreCase)) {
      Material = LoadObject<UPhysicalMaterial>(nullptr, *Path);
      if (!Material) {
        OutError = FString::Printf(TEXT("no physical material at '%s'"), *Path);
        return false;
      }
    }
    Primitive->SetPhysMaterialOverride(Material);
    return true;
  }
  if (Primitive && Name.EndsWith(TEXT("CollisionResponses"), ESearchCase::IgnoreCase)) {
    const TSharedPtr<FJsonObject> *Object = nullptr;
    TArray<TPair<ECollisionChannel, ECollisionResponse>> Responses;
    if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object) {
      OutError = TEXT("expects an object of channel: response, e.g. {\"Pawn\": \"Ignore\"}");
      return false;
    }
    if (!McpReadCollisionResponses(*Object, Responses, OutError)) {
      return false;
    }
    McpApplyCollisionResponses(Primitive, Responses);
    return true;
  }
  if (!Primitive || !Value.IsValid() || Value->Type != EJson::String) {
    OutError = TEXT("expects a string (a collision profile or ECollisionEnabled name)");
    return false;
  }
  const FString Text = Value->AsString();
  if (Name.EndsWith(TEXT("CollisionProfileName"), ESearchCase::IgnoreCase)) {
    if (!McpIsCollisionProfile(Text, OutError)) {
      return false;
    }
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
