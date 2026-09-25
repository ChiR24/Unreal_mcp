#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/Landscape/McpLandscapeMetadataTags.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
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

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

UMaterialInterface *LoadMaterialForMcp(const FString &MaterialPath,
                                       FString &OutResolvedPath,
                                       FString &OutError);
AActor *FindActorByNameInWorldForMcp(UWorld *World, const FString &Target,
                                     bool bExactMatchOnly);
// Appends per-component inspection fields (class identity, attach parent,
// visibility/active flags, and a bounded property name/type census) to an
// existing component-list entry. Defined in
// McpAutomationBridge_ControlActorComponentDetails.cpp; additive fields only.
void McpAppendComponentDetailFields(UActorComponent *Component,
                                    TSharedPtr<FJsonObject> &Entry);

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
  const FString ActorFolder = Actor->GetFolderPath().ToString();
  if (Folder == TEXT("(none)"))
    return ActorFolder.IsEmpty();
  return ActorFolder.Equals(Folder, ESearchCase::IgnoreCase) ||
         ActorFolder.StartsWith(Folder + TEXT("/"), ESearchCase::IgnoreCase);
}
#endif

// Placement diagnostics shared by spawn and transform: report what an actor
// ended up intersecting, and whether it is sunk into or floating above the
// surface beneath it, rather than answering a bare "success".
namespace McpPlacement {
#if WITH_EDITOR
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data);
/** True when a caller has marked this actor's placement deliberate. */
bool McpPlacementAccepted(const AActor *Actor);
#endif
} // namespace McpPlacement
