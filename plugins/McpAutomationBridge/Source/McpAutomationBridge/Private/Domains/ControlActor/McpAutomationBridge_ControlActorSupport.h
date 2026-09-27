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

// An actor's outliner folder, "" at the root. FName spells NAME_None "None", so
// ToString() alone counted the root as a folder named None in list's summary,
// and the "(none)" filter, which looked for "", matched no actor at all.
inline FString McpActorFolder(const AActor *Actor) {
  const FName Path = Actor->GetFolderPath();
  return Path.IsNone() ? FString() : Path.ToString();
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
    FString Wanted = FPackageName::ObjectPathToObjectName(ClassName);
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

// Placement diagnostics shared by spawn and transform: report what an actor
// ended up intersecting, and whether it is sunk into or floating above the
// surface beneath it, rather than answering a bare "success".
namespace McpPlacement {
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data);
/** True when a caller has marked this actor's placement deliberate. */
bool McpPlacementAccepted(const AActor *Actor);
} // namespace McpPlacement
