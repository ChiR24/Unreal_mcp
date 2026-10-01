// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/Placement/McpAutomationBridge_PlacementMount.h"

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

AActor *FindMount(UWorld *World, AActor *Actor, const FBox &ActorBox, const double Touch) {
  const FBox Reach = ActorBox.ExpandBy(Touch);
  const FVector Own = ActorBox.GetSize();
  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Other = *It;
    if (!Other || Other == Actor || Other->IsHidden() || IsBoundsOnlyActor(Other)) {
      continue;
    }
    FVector Origin = FVector::ZeroVector;
    FVector Extent = FVector::ZeroVector;
    Other->GetActorBounds(true, Origin, Extent);
    const FBox Shared = Reach.Overlap(FBox::BuildAABB(Origin, Extent));
    if (Extent.GetMin() <= 1.0 || !Shared.IsValid) {
      continue;
    }
    const FVector Size = Shared.GetSize();
    const bool bAcrossX = Size.X <= Size.Y;
    const double Depth = bAcrossX ? Size.X : Size.Y;
    if (Depth < Size.Z && Depth <= FMath::Max(2.0 * Touch, 0.25 * (bAcrossX ? Own.X : Own.Y))) {
      return Other;
    }
  }
  return nullptr;
}
} // namespace McpPlacement
