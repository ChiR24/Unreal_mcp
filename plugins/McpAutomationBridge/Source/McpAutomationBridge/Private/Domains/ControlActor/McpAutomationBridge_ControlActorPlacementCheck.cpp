// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

// A spawn or a transform write used to answer nothing but "success" and the
// coordinates it was handed back, so an actor buried to the waist in the floor,
// or intersecting a wall, or hanging in mid-air, read exactly like a correct
// placement. The caller only found out by looking at a screenshot -- which an
// automation caller often never takes.
//
// Blueprint node creation already refuses an overlapping position and names
// what it collided with (see the NODE_OVERLAP path). This is the same idea for
// world actors, except it warns rather than refuses: a deliberate overlap is
// legitimate in a level, an accidental one usually is not, and the caller is
// the one who can tell the difference.

namespace McpPlacement {
#if WITH_EDITOR

namespace {

/** Actors with no meaningful volume would report a bogus overlap against everything. */
bool McpHasUsableBounds(const FVector &Extent) {
  return Extent.X > 1.0 && Extent.Y > 1.0 && Extent.Z > 1.0;
}

/**
 * Volume-only actors (post-process, trigger, audio, kill-Z) legitimately enclose
 * everything inside them, so reporting those as overlaps would bury the real
 * signal under noise.
 */
bool McpIsBoundsOnlyActor(const AActor *Actor) {
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
         ClassName.EndsWith(TEXT("SubsystemRenderingActor"));
}

/**
 * A slab: far wider than it is thick. Floors, aprons, inlays, emblems, road
 * decals and platform tiers are all built this way, and a level assembles them
 * by STACKING them into each other -- a 24-thick floor disc with a decorative
 * inlay bedded 8 units into it is correct construction, not a mistake. Without
 * this distinction the hub reported 407 of 776 actors as broken, which is the
 * same as reporting nothing.
 */
bool McpIsSlab(const FVector &Extent) {
  return Extent.Z * 4.0 < FMath::Min(Extent.X, Extent.Y);
}


/**
 * Only a floor can tell you whether something is sunk. The ground trace used to
 * accept whatever it hit first on the way down from an actor's top, so a
 * neighbouring tree's canopy, a market awning or a roof overhang became "the
 * surface under it" -- and every actor standing beneath one was reported as sunk
 * by the height of the thing above it, with no overlap to explain why.
 */
bool McpIsGroundLike(const AActor *Actor) {
  if (!Actor) {
    return false;
  }
  if (Actor->GetClass()->GetName().Contains(TEXT("Landscape"))) {
    return true;
  }
  FVector GroundOrigin = FVector::ZeroVector;
  FVector GroundExtent = FVector::ZeroVector;
  Actor->GetActorBounds(true, GroundOrigin, GroundExtent);
  return McpIsSlab(GroundExtent);
}

/** Overlap along the shallowest axis -- how far the boxes actually interpenetrate. */
double McpPenetrationDepth(const FBox &A, const FBox &B) {
  const FBox Shared = A.Overlap(B);
  if (!Shared.IsValid) {
    return 0.0;
  }
  const FVector Size = Shared.GetSize();
  return FMath::Min3(Size.X, Size.Y, Size.Z);
}

} // namespace

/**
 * A geometric test cannot tell a mistake from a composition. A keep is built by
 * bedding its towers, walls and stairs into its platform; an island is meant to
 * hang in the air; a jumbotron is meant to hang off a mast. Left alone, those
 * report forever and train the caller to ignore the whole check. This tag is the
 * caller's way to say "checked, deliberate" -- add it with control_actor.add_tag
 * and the actor drops out as both subject and overlap target, so the flagged
 * count can actually reach zero and mean something.
 */
bool McpPlacementAccepted(const AActor *Actor) {
  return Actor && Actor->ActorHasTag(FName(TEXT("mcp.placement.ok")));
}

/**
 * Describe where Actor actually ended up: what it interpenetrates, and whether
 * it is sunk into or floating above the surface under it. Adds placementWarning,
 * overlappingActors[] and suggestedLocation to Data when there is something to
 * say, and leaves Data untouched when the placement looks clean.
 */
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data) {
  if (!Actor || !Data.IsValid()) {
    return;
  }
  UWorld *World = Actor->GetWorld();
  if (!World) {
    return;
  }
  // The same actors that make useless overlap TARGETS make useless subjects: a
  // level-spanning debug-draw proxy reported itself as sunk into the geometry it
  // was drawn over, and led the list every time.
  if (McpIsBoundsOnlyActor(Actor) || McpPlacementAccepted(Actor)) {
    return;
  }

  FVector Origin = FVector::ZeroVector;
  FVector Extent = FVector::ZeroVector;
  Actor->GetActorBounds(true, Origin, Extent);
  if (!McpHasUsableBounds(Extent)) {
    return;
  }
  const FBox SelfBox = FBox::BuildAABB(Origin, Extent);

  // Ignore a graze: meshes are authored to touch, and a shared edge is not a bug.
  // Scale the floor with the actor so a large building is not flagged for the
  // same absolute overlap that matters on a character.
  const double IgnoreBelow =
      FMath::Max(4.0, FMath::Min3(Extent.X, Extent.Y, Extent.Z) * 0.12);
  const bool bSelfSlab = McpIsSlab(Extent);

  TArray<TSharedPtr<FJsonValue>> Overlaps;
  double WorstDepth = 0.0;
  FString WorstName;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Other = *It;
    if (!Other || Other == Actor || Other->IsHidden() ||
        McpIsBoundsOnlyActor(Other) || McpPlacementAccepted(Other)) {
      continue;
    }
    // An attached child sharing its parent's space is structural, not a mistake.
    if (Other->IsAttachedTo(Actor) || Actor->IsAttachedTo(Other)) {
      continue;
    }

    FVector OtherOrigin = FVector::ZeroVector;
    FVector OtherExtent = FVector::ZeroVector;
    Other->GetActorBounds(true, OtherOrigin, OtherExtent);
    if (!McpHasUsableBounds(OtherExtent)) {
      continue;
    }

    const FBox OtherBox = FBox::BuildAABB(OtherOrigin, OtherExtent);
    const double Depth = McpPenetrationDepth(SelfBox, OtherBox);
    if (Depth <= IgnoreBelow) {
      continue;
    }
    // Two slabs bedded into each other is how a tiered floor is built. Only
    // call it a fault once one has swallowed the other's whole thickness.
    if (bSelfSlab && McpIsSlab(OtherExtent) &&
        Depth <= 2.0 * FMath::Min(Extent.Z, OtherExtent.Z)) {
      continue;
    }

    if (Overlaps.Num() < 8) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("actorName"), Other->GetActorLabel());
      Entry->SetStringField(TEXT("actorClass"), Other->GetClass()->GetName());
      Entry->SetNumberField(TEXT("penetrationDepth"), FMath::RoundToDouble(Depth));
      Overlaps.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if (Depth > WorstDepth) {
      WorstDepth = Depth;
      WorstName = Other->GetActorLabel();
    }
  }

  // Where the ground actually is, measured from just under the actor's feet so
  // the trace does not start inside its own collision.
  const double BottomZ = Origin.Z - Extent.Z;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(McpPlacementGround), false, Actor);
  const FVector TraceStart(Origin.X, Origin.Y, Origin.Z + Extent.Z);
  const FVector TraceEnd(Origin.X, Origin.Y, BottomZ - 100000.0);

  bool bHasGround = false;
  double GroundZ = 0.0;
  TArray<FHitResult> Hits;
  if (World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_WorldStatic,
                                     Params)) {
    // Hits come back ordered along the ray, which points down, so the first
    // floor-shaped thing struck is the highest one under the actor.
    for (const FHitResult &Candidate : Hits) {
      if (!McpIsGroundLike(Candidate.GetActor())) {
        continue;
      }
      bHasGround = true;
      GroundZ = Candidate.ImpactPoint.Z;
      break;
    }
  }

  TArray<FString> Notes;
  if (Overlaps.Num() > 0) {
    Data->SetArrayField(TEXT("overlappingActors"), Overlaps);
    Notes.Add(FString::Printf(
        TEXT("intersects %d actor(s), deepest '%s' by %.0f units"),
        Overlaps.Num(), *WorstName, WorstDepth));
  }

  if (bHasGround) {
    const double Clearance = BottomZ - GroundZ;
    Data->SetNumberField(TEXT("groundZ"), FMath::RoundToDouble(GroundZ));
    Data->SetNumberField(TEXT("groundClearance"), FMath::RoundToDouble(Clearance));

    // A slab set into the tier below it is inlay work; only a slab swallowed
    // deeper than its own thickness is actually lost in the geometry.
    const double SunkFloor =
        bSelfSlab ? FMath::Max(IgnoreBelow, 2.0 * Extent.Z) : IgnoreBelow;
    if (Clearance < -SunkFloor) {
      // The exact trap that buried a Character: its location is the capsule
      // CENTRE, so reusing a StaticMeshActor's feet-relative Z sinks it by half
      // its height. Hand back the Z that actually rests on the surface.
      const double SuggestedZ = GroundZ + Extent.Z;
      TSharedPtr<FJsonObject> Suggested = MakeShared<FJsonObject>();
      Suggested->SetNumberField(TEXT("x"), Origin.X);
      Suggested->SetNumberField(TEXT("y"), Origin.Y);
      Suggested->SetNumberField(TEXT("z"), FMath::RoundToDouble(SuggestedZ));
      Data->SetObjectField(TEXT("suggestedLocation"), Suggested);
      Notes.Add(FString::Printf(
          TEXT("sunk %.0f units below the surface under it; an actor's location "
               "is its bounds centre, not its base -- z=%.0f would rest on it"),
          -Clearance, SuggestedZ));
    } else if (Clearance > 50.0) {
      Notes.Add(FString::Printf(
          TEXT("floating %.0f units above the surface under it"), Clearance));
    }
  } else {
    Notes.Add(TEXT("nothing below it - it may be outside the playable area"));
  }

  if (Notes.Num() > 0) {
    Data->SetStringField(
        TEXT("placementWarning"),
        FString::Printf(TEXT("'%s' %s."), *Actor->GetActorLabel(),
                        *FString::Join(Notes, TEXT("; "))));
  }
}

#endif
} // namespace McpPlacement
