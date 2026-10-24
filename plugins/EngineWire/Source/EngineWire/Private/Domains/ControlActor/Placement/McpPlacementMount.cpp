// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpPlacementMount.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace McpPlacement {
bool IsBoundsOnlyActor(const AActor *Actor) {
  if (!Actor) {
    return true;
  }
  const FString ClassName = Actor->GetClass()->GetName();
  return ClassName.Contains(TEXT("Volume")) ||
         ClassName.Contains(TEXT("Trigger")) ||
         ClassName.Contains(TEXT("Fog")) ||
         ClassName.Contains(TEXT("Light")) ||
         ClassName.Contains(TEXT("PlayerStart")) ||
         ClassName.Contains(TEXT("WorldSettings")) ||
         ClassName.Contains(TEXT("Brush")) ||
         // Subsystem debug-draw proxies (SmartObject, navigation, mass entity)
         // carry bounds spanning the whole level, so without this every actor
         // reports an overlap against them and the real findings drown.
         ClassName.Contains(TEXT("RenderingActor")) ||
         ClassName.Contains(TEXT("Subsystem")) ||
         // PIE spawns one per player; every teleport in a play session read
         // "intersects GameplayDebuggerCategoryReplicator0 by 64 units".
         ClassName.Contains(TEXT("GameplayDebugger")) ||
         ClassName.EndsWith(TEXT("SubsystemRenderingActor"));
}

namespace {
bool IsDrawnPart(const UPrimitiveComponent *Part) {
  return Part->IsRegistered() && Part->IsVisible() && !Part->bHiddenInGame && !Part->IsEditorOnly();
}
} // namespace

FBox DrawnBounds(const AActor &Actor) {
  FBox Box(ForceInit);
  TInlineComponentArray<UPrimitiveComponent *> Parts(&Actor);
  for (const UPrimitiveComponent *Part : Parts) {
    if (IsDrawnPart(Part)) {
      Box += Part->Bounds.GetBox();
    }
  }
  return Box.IsValid ? Box : Actor.GetComponentsBoundingBox(true);
}

FBox DrawnBoundsIn(const AActor &Actor, const FTransform &Frame) {
  FBox Box(ForceInit);
  TInlineComponentArray<UPrimitiveComponent *> Parts(&Actor);
  for (const UPrimitiveComponent *Part : Parts) {
    if (IsDrawnPart(Part)) {
      Box += Part->CalcBounds(Part->GetComponentTransform().GetRelativeTransform(Frame)).GetBox();
    }
  }
  return Box;
}

bool HangsOn(const FBox &ActorBox, const FBox &OtherBox, const double Touch) {
  const FBox Shared = ActorBox.ExpandBy(Touch).Overlap(OtherBox);
  if (OtherBox.GetExtent().GetMin() <= 1.0 || !Shared.IsValid) {
    return false;
  }
  const FVector Own = ActorBox.GetSize();
  const FVector Size = Shared.GetSize();
  const bool bAcrossX = Size.X <= Size.Y;
  const double Depth = bAcrossX ? Size.X : Size.Y;
  return Depth < Size.Z && Depth <= FMath::Max(2.0 * Touch, 0.25 * (bAcrossX ? Own.X : Own.Y));
}

AActor *FindMount(UWorld *World, AActor *Actor, const FBox &ActorBox, const double Touch) {
  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Other = *It;
    if (!Other || Other == Actor || Other->IsHidden() || IsBoundsOnlyActor(Other)) {
      continue;
    }
    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
    Other->GetActorBounds(true, Origin, Extent);
    if (HangsOn(ActorBox, FBox::BuildAABB(Origin, Extent), Touch)) {
      return Other;
    }
  }
  return nullptr;
}
} // namespace McpPlacement
