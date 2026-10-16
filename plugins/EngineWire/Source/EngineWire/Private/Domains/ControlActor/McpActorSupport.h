#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/EngineWireGlobals.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exporters/Exporter.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"

#include "Subsystems/EditorActorSubsystem.h"

UMaterialInterface *LoadMaterialForMcp(const FString &MaterialPath,
                                       FString &OutResolvedPath,
                                       FString &OutError);
// Appends per-component inspection fields (class identity, attach parent,
// visibility/active flags, and a bounded property name/type census) to an
// existing component-list entry. Defined in
// McpActorComponentDetails.cpp; additive fields only.
void McpAppendComponentDetailFields(UActorComponent *Component,
                                    TSharedPtr<FJsonObject> &Entry);

// The component property bag add_component and set_component_properties share
// (McpActorComponentProps.cpp): Mobility first, the
// engine setters for physics, mesh and collision, then McpResolvePropertyPath.
void McpApplyComponentProperties(UActorComponent *Component, const TSharedPtr<FJsonObject> &Properties,
                                 TArray<FString> &OutApplied, TArray<FString> &OutWarnings);
// A spawn_batch item's properties on the actor it spawned (its root component, or componentName); "" when every one
// applied or none was given, else what did not.
FString McpApplySpawnItemProperties(AActor *Actor, const TSharedPtr<FJsonObject> &Item);
// True for a key McpApplyComponentProperties writes through the component's own setter (Mobility, SimulatePhysics, a
// StaticMeshComponent's StaticMesh, collision): a raw reflected write of one skips what the setter keeps in step.
bool McpIsComponentSetterKey(const UActorComponent *Component, const FString &Name);
// Sends PROPERTY_CONVERSION_FAILED (nothing applied) or PARTIAL_FAILURE with the
// warnings in Data and returns true; false (nothing sent) when all applied.
bool McpSendComponentPropertyShortfall(UEngineWireSubsystem &Bridge,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                                       const TArray<FString> &Applied, const TArray<FString> &Warnings,
                                       const TSharedPtr<FJsonObject> &Data, const FString &Prefix = FString());
// set_component_properties with actorNames, already resolved (McpActorComponentDetails.cpp): writes the reply.
void McpSetComponentPropertiesOnActors(UEngineWireSubsystem &Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                       const FString &RequestId, const TArray<AActor *> &Actors,
                                       const TArray<FString> &Missing, const FString &ComponentName,
                                       const TSharedPtr<FJsonObject> &Properties);

// An actor's outliner folder, "" at the root. FName spells NAME_None "None", so
// ToString() alone counted the root as a folder named None in list's summary,
// and the "(none)" filter, which looked for "", matched no actor at all.
inline FString McpActorFolder(const AActor *Actor) {
  const FName Path = Actor->GetFolderPath();
  return Path.IsNone() ? FString() : Path.ToString();
}

// A property named for the whole actor or for one of its components: "bDead" is read off the actor,
// "Visual.RelativeScale3D" off the component FindComponentByName finds under that name (a squash on
// landing lives on a component, not the actor), and "CharacterMovement.MaxWalkSpeed" off the object the
// actor's own property of that name holds, as get_property reads it (the component is named CharMoveComp).
// One resolver for the propertyNames of sample_motion and of list. OutOwner is the object to read the
// property from; null Property when nothing matches.
inline FProperty *McpResolveActorPropertyPath(AActor *Actor, const FString &Wanted, UObject *&OutOwner) {
  FString ComponentName, PropertyName = Wanted;
  OutOwner = Actor;
  if (Wanted.Split(TEXT("."), &ComponentName, &PropertyName)) {
    const FObjectPropertyBase *Holder = FindFProperty<FObjectPropertyBase>(Actor->GetClass(), FName(*ComponentName));
    OutOwner = Holder ? Holder->GetObjectPropertyValue_InContainer(Actor) : nullptr;
    if (!OutOwner) {
      OutOwner = FindComponentByName(Actor, ComponentName);
    }
  }
  return OutOwner ? OutOwner->GetClass()->FindPropertyByName(FName(*PropertyName)) : nullptr;
}

// An actor variable, or an array of them, takes the actors by label or name, as every other call names actors: an
// array took paths only, so FireFx ["S6_RackFire_1", ...] needed each fire's /Game/...:PersistentLevel.NiagaraActor_6.
inline TSharedPtr<FJsonValue> McpActorRefsToPaths(const TSharedPtr<FJsonValue> &Value, const FProperty *Property,
                                                  TFunctionRef<AActor *(const FString &)> Find) {
  const FArrayProperty *Array = CastField<FArrayProperty>(Property);
  const FObjectProperty *Reference = CastField<FObjectProperty>(Array ? Array->Inner : Property);
  if (!Value.IsValid() || !Reference || !Reference->PropertyClass || !Reference->PropertyClass->IsChildOf(AActor::StaticClass()))
    return Value;
  const auto ToPath = [&Find](const TSharedPtr<FJsonValue> &Item) -> TSharedPtr<FJsonValue> {
    const FString Name = Item.IsValid() && Item->Type == EJson::String ? Item->AsString() : FString();
    AActor *Actor = !Name.IsEmpty() && !Name.Contains(TEXT("/")) ? Find(Name) : nullptr;
    return Actor ? MakeShared<FJsonValueString>(Actor->GetPathName()) : Item;
  };
  if (!Array || Value->Type != EJson::Array)
    return Array ? Value : ToPath(Value);
  TArray<TSharedPtr<FJsonValue>> Items;
  for (const TSharedPtr<FJsonValue> &Item : Value->AsArray())
    Items.Add(ToPath(Item));
  return MakeShared<FJsonValueArray>(Items);
}

// control_actor.list's structural filters. Finding every TextRenderActor, or
// what an outliner folder holds, used to mean guessing label substrings. Tag:
// the actor carries it. ClassName: the actor's class or any parent, by name or
// path, a Blueprint's "_C" optional. Folder: that folder or one under it,
// "(none)" for the root. An empty argument matches every actor.
inline bool McpActorMatchesListFilters(const AActor *Actor, const FString &Tag,
                                       const FString &ClassName, const FString &Folder) {
  if (!Tag.IsEmpty() && !Actor->ActorHasTag(FName(*Tag)))
    return false;
  if (!ClassName.IsEmpty()) {
    // GetShortName: a package path (/Game/Enemies/BP_Enemy) has no object part, and
    // ObjectPathToObjectName hands the whole path back, which matched no class.
    FString Wanted = FPackageName::GetShortName(FPackageName::ObjectPathToObjectName(ClassName));
    Wanted.RemoveFromEnd(TEXT("_C"));
    bool bClassMatch = false;
    for (const UClass *Class = Actor->GetClass(); Class && !bClassMatch; Class = Class->GetSuperClass()) {
      FString Name = Class->GetName();
      Name.RemoveFromEnd(TEXT("_C"));
      bClassMatch = Name.Equals(Wanted, ESearchCase::IgnoreCase);
    }
    if (!bClassMatch)
      return false;
  }
  if (Folder.IsEmpty())
    return true;
  const FString ActorFolder = McpActorFolder(Actor);
  if (Folder == TEXT("(none)"))
    return ActorFolder.IsEmpty();
  return ActorFolder.Equals(Folder, ESearchCase::IgnoreCase) ||
         ActorFolder.StartsWith(Folder + TEXT("/"), ESearchCase::IgnoreCase);
}

// The many-actor form of an action: the actors an `actorNames` array names, each once and in order,
// and the names that matched no actor. False, with nothing filled, when the payload has no
// non-empty actorNames. Find is the subsystem's FindActorByName (a private member, so each handler
// hands it in). add_tag, set_visibility and set_actor_collision all take the list through this.
inline bool McpResolveActorNames(const TSharedPtr<FJsonObject> &Payload,
                                 TFunctionRef<AActor *(const FString &)> Find,
                                 TArray<AActor *> &OutActors, TArray<FString> &OutMissing) {
  const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
  for (const FString &Name : Names) {
    if (AActor *Actor = Find(Name)) {
      OutActors.AddUnique(Actor);
    } else {
      OutMissing.Add(Name);
    }
  }
  return Names.Num() > 0;
}

// The refusal for an actorNames list none of whose names matched an actor; they go back in `missing`.
inline void McpSendNoActorNamesFound(UEngineWireSubsystem *Bridge,
                                     TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                                     const TArray<FString> &Missing) {
  TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
  Details->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
  SendStandardErrorResponse(Bridge, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                            FString::Printf(TEXT("None of actorNames was found: %s"),
                                            *FString::Join(Missing, TEXT(", "))),
                            Details);
}

// set_visibility, one actor: its own flags, then every primitive component's. True when the actor
// reads back as asked.
inline bool McpApplyActorVisibility(AActor *Actor, bool bVisible) {
  Actor->SetActorHiddenInGame(!bVisible);
  Actor->SetActorEnableCollision(bVisible);
  for (UActorComponent *Comp : Actor->GetComponents()) {
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }
  Actor->MarkComponentsRenderStateDirty();
  Actor->MarkPackageDirty();
  return Actor->IsHidden() == !bVisible;
}

// What a change to an actor and its primitive components mutates, for the undo transaction of
// set_visibility and set_actor_collision: the actor and each primitive component (only the actor
// would let undo restore half the change), each flagged RF_Transactional when it is not. A
// component the engine made without the flag (a Blueprint actor's BillboardComponent) never
// reaches the undo buffer, so the transaction gate refused the whole set and the receipt said
// undoable:false; flagging it first (as add_component does for the components it makes) lets
// Modify() record it.
inline void McpAddActorUndoSet(AActor *Actor, TArray<UObject *> &Undoable) {
  const auto Add = [&Undoable](UObject *Object) {
    if (!Object->HasAnyFlags(RF_Transactional)) {
      Object->SetFlags(RF_Transactional);
    }
    Undoable.Add(Object);
  };
  Add(Actor);
  for (UActorComponent *Comp : Actor->GetComponents()) {
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Add(Prim);
    }
  }
}

// The undo step of a spawn, "Spawn Actors": a spawn ran outside any transaction, so undo answered
// NOTHING_TO_UNDO and the actors stayed. A single spawn opens it before it touches the level and
// describes it in its reply; a run inside spawn_batch (captured) gets none, because the batch holds
// one for every item.
inline TUniquePtr<FMcpScopedEditorTransaction> McpBeginSpawnTransaction(const FString &RequestId) {
  if (FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) {
    return nullptr;
  }
  return MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(TEXT("Spawn Actors")),
                                                  EMcpMutationDurability::EditorStateOnly, TArray<UObject *>());
}

// SpawnActor for the spawn handlers, noting whether the actor began to spawn: one whose BeginPlay, or an overlap where
// it appeared, destroys it at once (a pickup collected on the spot) leaves SpawnActor null exactly like a spawn that
// never started, and was reported as a failed spawn.
inline AActor *McpSpawnActorNoting(UWorld *World, UClass *Class, const FVector &Location, const FRotator &Rotation,
                                   const FActorSpawnParameters &Params, bool &bOutStarted) {
  bOutStarted = false;
  const FDelegateHandle Seen = World->AddOnActorPreSpawnInitialization(
      FOnActorSpawned::FDelegate::CreateLambda([&bOutStarted](AActor *) { bOutStarted = true; }));
  AActor *Spawned = World->SpawnActor(Class, &Location, &Rotation, Params);
  World->RemoveOnActorPreSpawnInitialization(Seen);
  return Spawned;
}

inline FString McpSpawnFailureCode(bool bStarted) {
  return bStarted ? TEXT("SPAWNED_THEN_DESTROYED") : TEXT("SPAWN_FAILED");
}

inline FString McpSpawnFailureMessage(bool bStarted, const TCHAR *NeverStarted) {
  return bStarted ? FString(TEXT("The actor spawned and destroyed itself at once: its BeginPlay, or an overlap where "
                                 "it was spawned (a pickup collected on the spot), ended it"))
                  : FString(NeverStarted);
}

// sample_motion's timeline (McpActorMotionInputs.cpp).
// A caller timing a jump over two calls lost 1-2 game seconds to its own delay
// between them, so the timing has to live inside the run: keys pressed at game
// offsets from its start, and a start held until another actor's property
// takes a value (a platform appearing).
struct FMcpMotionInput {
  FString Key;
  double At = 0.0, Hold = 0.1, DownAt = -1.0, UpAt = -1.0;
  uint64 DownFrame = 0;
  // The game widget that held keyboard focus when the key went down (a menu's button takes it first).
  FString Focused;
};
struct FMcpMotionTrigger {
  TWeakObjectPtr<AActor> Actor;
  // By path ("bDead", "CharacterMovement.MovementMode"), resolved on each read as propertyNames are: a
  // Blueprint recompiled mid-run frees the FProperty it had.
  FString Property;
  FString Equals, Last;
  bool bWaitForChange = true, bSeen = false;
  double WaitStart = 0.0, Deadline = 0.0;
};
bool McpParseMotionInputs(const TSharedPtr<FJsonObject> &Payload,
                          TArray<FMcpMotionInput> &Out, FString &Error);
bool McpInitMotionTrigger(AActor *Gate, const TSharedPtr<FJsonObject> &When, UWorld *World,
                          FMcpMotionTrigger &Out, FString &Error);
bool McpMotionTriggerFired(FMcpMotionTrigger &Trigger);
FString McpStartWhenTimeoutWarning(const FMcpMotionTrigger &Trigger);
FString McpGamePausedWarning(const UWorld *World);
FString McpIgnoredInputsWarning(const AActor *Actor, const TArray<FMcpMotionInput> &Inputs, int32 SampleCount,
                                const FBox &Extent);
FString McpSlowFrameWarning(double GameSeconds, int32 Frames);
void McpApplyMotionInputs(TArray<FMcpMotionInput> &Inputs, double Elapsed, bool bRunEnded);
TArray<TSharedPtr<FJsonValue>> McpMotionInputsJson(const TArray<FMcpMotionInput> &Inputs);

// Placement diagnostics shared by spawn and transform: report what an actor
// ended up intersecting, and whether it is sunk into or floating above the
// surface beneath it, rather than answering a bare "success".
namespace McpPlacement {
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data);
/** True when a caller has marked this actor's placement deliberate. */
bool McpPlacementAccepted(const AActor *Actor);
} // namespace McpPlacement

// A spawn's outliner folder and actor tags (folder, tags), for a single spawn and a spawn_batch item alike; applied before
// the placement is judged, so a tag of mcp.placement.ok counts.
void McpApplySpawnOrganisation(AActor *Actor, const TSharedPtr<FJsonObject> &Payload);

// A spawn or a move answers where the actor ended up, and which faces of it share a plane with another (z-fighting). Run
// as an item of spawn_batch or of set_transform's actors (captured), it leaves both to the batch, which checks once the
// whole layout stands (a spawn after tagging the actor mcp.placement.ok): each check walks every actor in the level.
inline void McpDescribeActorPlacement(const FString &RequestId, AActor *Actor, const TSharedPtr<FJsonObject> &Data) {
  if (!FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) {
    McpPlacement::DescribePlacement(Actor, Data);
    TMap<AActor *, TSharedPtr<FJsonObject>> Row;
    Row.Add(Actor, Data);
    McpCoplanar::AddCoplanarToRows(Row);
  }
}
