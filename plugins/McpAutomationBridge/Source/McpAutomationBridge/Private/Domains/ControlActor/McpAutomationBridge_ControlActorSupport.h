#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
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
// McpAutomationBridge_ControlActorComponentDetails.cpp; additive fields only.
void McpAppendComponentDetailFields(UActorComponent *Component,
                                    TSharedPtr<FJsonObject> &Entry);

// The component property bag add_component and set_component_properties share
// (McpAutomationBridge_ControlActorComponentProperties.cpp): Mobility first, the
// engine setters for physics, mesh and collision, then McpResolvePropertyPath.
void McpApplyComponentProperties(UActorComponent *Component, const TSharedPtr<FJsonObject> &Properties,
                                 TArray<FString> &OutApplied, TArray<FString> &OutWarnings);
// Sends PROPERTY_CONVERSION_FAILED (nothing applied) or PARTIAL_FAILURE with the
// warnings in Data and returns true; false (nothing sent) when all applied.
bool McpSendComponentPropertyShortfall(UMcpAutomationBridgeSubsystem &Bridge,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                                       const TArray<FString> &Applied, const TArray<FString> &Warnings,
                                       const TSharedPtr<FJsonObject> &Data, const FString &Prefix = FString());

// An actor's outliner folder, "" at the root. FName spells NAME_None "None", so
// ToString() alone counted the root as a folder named None in list's summary,
// and the "(none)" filter, which looked for "", matched no actor at all.
inline FString McpActorFolder(const AActor *Actor) {
  const FName Path = Actor->GetFolderPath();
  return Path.IsNone() ? FString() : Path.ToString();
}

// A property named for the whole actor or for one of its components: "bDead" is read off the actor,
// "Visual.RelativeScale3D" off the component FindComponentByName finds under that name (a squash on
// landing lives on a component, not the actor). One resolver for the propertyNames of sample_motion
// and of list. OutOwner is the object to read the property from; null Property when nothing matches.
inline FProperty *McpResolveActorPropertyPath(AActor *Actor, const FString &Wanted, UObject *&OutOwner) {
  FString ComponentName, PropertyName = Wanted;
  OutOwner = Actor;
  if (Wanted.Split(TEXT("."), &ComponentName, &PropertyName)) {
    OutOwner = FindComponentByName(Actor, ComponentName);
  }
  return OutOwner ? OutOwner->GetClass()->FindPropertyByName(FName(*PropertyName)) : nullptr;
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
    // GetShortName: a package path (/Game/Enemies/BP_Bug) has no object part, and
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
inline void McpSendNoActorNamesFound(UMcpAutomationBridgeSubsystem *Bridge,
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

// What that change mutates, for the undo transaction: the actor and each primitive component.
// Capturing only the actor would let undo restore half the change.
inline void McpAddVisibilityUndoSet(AActor *Actor, TArray<UObject *> &Undoable) {
  Undoable.Add(Actor);
  for (UActorComponent *Comp : Actor->GetComponents()) {
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Undoable.Add(Prim);
    }
  }
}

// sample_motion's timeline (McpAutomationBridge_ControlActorMotionInputs.cpp).
// A caller timing a jump over two calls lost 1-2 game seconds to its own delay
// between them, so the timing has to live inside the run: keys pressed at game
// offsets from its start, and a start held until another actor's property
// takes a value (a platform appearing).
struct FMcpMotionInput {
  FString Key;
  double At = 0.0, Hold = 0.1, DownAt = -1.0, UpAt = -1.0;
  uint64 DownFrame = 0;
};
struct FMcpMotionTrigger {
  TWeakObjectPtr<AActor> Actor;
  // By name, looked up on each read: a Blueprint recompiled mid-run frees the FProperty it had.
  FName Property;
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
