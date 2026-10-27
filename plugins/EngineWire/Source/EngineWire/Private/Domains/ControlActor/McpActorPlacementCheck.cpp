// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Placement/McpChangeWatch.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/ControlActor/Placement/McpPlacementShapes.h"

#include "Components/CapsuleComponent.h"
#include "Engine/NavigationObjectBase.h"
#include "GameFramework/Volume.h"
#include "LandscapeProxy.h"

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

namespace {

// Actors with no meaningful volume would report a bogus overlap against everything.
bool McpHasUsableBounds(const FVector &Extent) {
  return Extent.X > 1.0 && Extent.Y > 1.0 && Extent.Z > 1.0;
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

// A water body is a surface, not a solid: a buoy, a hull or a wading pawn sits partly under it on purpose, and
// "intersects the ocean by 47; sunk 47 below the surface; z=-3 would rest it" lifted a buoy on top of the sea.
// Matched by class name so the plugin needs no link to the Water module.
bool McpIsWaterBody(const AActor *Actor) {
  for (const UClass *Class = Actor ? Actor->GetClass() : nullptr; Class; Class = Class->GetSuperClass()) {
    if (Class->GetFName() == TEXT("WaterBody")) {
      return true;
    }
  }
  return false;
}

// A pickup, a trigger or a flying enemy is built from overlap bodies alone: it
// holds nothing up and nothing holds it up, so it hovers where it was put on
// purpose and "floating 275 units above the surface" told the caller nothing.
// A prop with no body at all is still judged by its bounds.
bool McpIsOverlapOnly(const AActor *Actor) {
  bool bAnyBody = false;
  TInlineComponentArray<UPrimitiveComponent *> Comps(Actor);
  for (const UPrimitiveComponent *Comp : Comps) {
    if (!Comp->IsCollisionEnabled() || !McpHasBody(Comp)) {
      continue;
    }
    if (McpBlocksSolids(Comp)) {
      return false;
    }
    bAnyBody = true;
  }
  return bAnyBody;
}

// The floor under Sample, traced down from TopZ, the actor's top (the actor itself is ignored, so a surface it is sunk
// into is still struck): the first hit that can hold the actor up there.
bool McpGroundUnder(UWorld *World, AActor *Actor, double TopZ, const FMcpGroundSample &Sample, double &OutGroundZ,
                    bool &bOutOnWater) {
  const FVector Centre = Sample.Box.GetCenter();
  const double BottomZ = Sample.Box.Min.Z;
  FCollisionQueryParams Params(SCENE_QUERY_STAT(McpPlacementGround), false, Actor);

  // Every hit along the ray, not just those up to the first block: a channel
  // trace ended at a pipe's lip resting on its body, so the floor beneath was
  // never seen and the body read "nothing below it".
  // A simulated body is a PhysicsBody: a crate stacked on a physics crate read "floating" above the floor under both.
  FCollisionObjectQueryParams Solids;
  Solids.AddObjectTypesToQuery(ECC_WorldStatic);
  Solids.AddObjectTypesToQuery(ECC_WorldDynamic);
  Solids.AddObjectTypesToQuery(ECC_PhysicsBody);
  TArray<FHitResult> Hits;
  World->LineTraceMultiByObjectType(Hits, FVector(Centre.X, Centre.Y, TopZ),
                                    FVector(Centre.X, Centre.Y, BottomZ - 100000.0), Solids, Params);

  // Hits come back ordered along the ray, which points down, so the first
  // floor struck is the highest one under the actor.
  for (const FHitResult &Candidate : Hits) {
    const UPrimitiveComponent *Surface = Candidate.GetComponent();
    AActor *HitActor = Candidate.GetActor();
    // Triggers and pickups share those object types; only something solid can
    // hold an actor up. An invisible volume holds up nothing a viewer sees, and a
    // trace starting inside one hits at its own start: the floor slab a blocking
    // start wall stands in read "sunk 160 units below the surface under it".
    if (!Surface || !HitActor || HitActor->IsA<AVolume>() ||
        Surface->GetCollisionResponseToChannel(ECC_WorldStatic) != ECR_Block) {
      continue;
    }
    FVector HitOrigin = FVector::ZeroVector;
    FVector HitExtent = FVector::ZeroVector;
    HitActor->GetActorBounds(true, HitOrigin, HitExtent);
    const double HitBottomZ = HitOrigin.Z - HitExtent.Z;
    // Something the actor holds up is not the floor under it: a pipe's lip on
    // its body, or a shelf deck its legs poke 12 units into (the trace starts
    // inside the deck), read as "the surface under it", so the body or leg was
    // sunk by its own full height and suggestedZ put it on top of the deck.
    if (HitBottomZ >= Centre.Z) {
      continue;
    }
    // Nor is what the actor encloses: a cart corral spawned around its carts read "sunk 97 units" below the carts'
    // tops. A floor under the actor is never inside its box; a crate under a crate lies below it.
    if (Sample.Box.ExpandBy(1.0).IsInside(FBox::BuildAABB(HitOrigin, HitExtent))) {
      continue;
    }
    // A floor is floor-shaped, or it reaches down past the actor's base to hold
    // it up: a side-scroller's ground strip, a stair block, a crate. The slab
    // shape alone left nearly every actor in a platformer "nothing below it".
    if (!McpIsGroundLike(HitActor) && HitBottomZ > BottomZ) {
      continue;
    }
    // A road section's line starts at the road's top, through the canopies and roofs over that section: above the part
    // judged, only ground that buries it counts. A single box's line starts at its own top.
    // ponytail: another slab-shaped road passing over a section (an overpass) counts as ground: HitBottomZ is its whole
    // route's lowest point. Judge a spline mesh hit by the struck component's bounds if overpasses need checking.
    if (Candidate.ImpactPoint.Z > Sample.Box.Max.Z && !McpIsGroundLike(HitActor)) {
      continue;
    }
    bOutOnWater = McpIsWaterBody(HitActor);
    OutGroundZ = Candidate.ImpactPoint.Z;
    return true;
  }
  return false;
}

} // namespace

// Only a floor can tell you whether something is sunk. The ground trace used to
// accept whatever it hit first on the way down from an actor's top, so a
// neighbouring tree's canopy, a market awning or a roof overhang became "the
// surface under it" -- and every actor standing beneath one was reported as sunk
// by the height of the thing above it, with no overlap to explain why.
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

// A geometric test cannot tell a mistake from a composition. A keep is built by
// bedding its towers, walls and stairs into its platform; an island is meant to
// hang in the air; a jumbotron is meant to hang off a mast. Left alone, those
// report forever and train the caller to ignore the whole check. This tag is the
// caller's way to say "checked, deliberate" -- add it with control_actor.add_tag
// and the actor drops out as both subject and overlap target, so the flagged
// count can actually reach zero and mean something.
bool McpPlacementAccepted(const AActor *Actor) {
  return Actor && Actor->ActorHasTag(FName(TEXT("mcp.placement.ok")));
}

// A part reshaped during this frame keeps the bounds of its old shape until its render state is rebuilt at the end of
// the frame: the spline meshes of a road just laid, bent after their mesh was set, read as stock 100-unit segments at
// the actor's origin, so the reply that laid a road over a landscape said "nothing below it".
void McpRefreshBounds(AActor *Actor) {
  TInlineComponentArray<UPrimitiveComponent *> Parts(Actor);
  for (UPrimitiveComponent *Part : Parts) {
    if (Part->IsRegistered()) {
      Part->UpdateBounds();
    }
  }
}

// Describe where Actor actually ended up: what it interpenetrates, and whether
// it is sunk into or floating above the surface under it. Adds placementWarning,
// overlappingActors[] and suggestedLocation to Data when there is something to
// say, and leaves Data untouched when the placement looks clean.
void DescribePlacement(AActor *Actor, const TSharedPtr<FJsonObject> &Data) {
  McpChangeWatch::NotePlacementReported(Actor); // the reply reports it: the change watch checks only what it shows
  if (!Actor || !Data.IsValid()) {
    return;
  }
  UWorld *World = Actor->GetWorld();
  if (!World) {
    return;
  }
  // The same actors that make useless overlap TARGETS make useless subjects: a
  // level-spanning debug-draw proxy reported itself as sunk into the geometry it
  // was drawn over, and led the list every time. An actor of overlap bodies alone
  // can neither interpenetrate nor rest on anything, so it has nothing to report.
  // A player start collides with nothing (its capsule only tests where a pawn fits), so it was skipped like a volume and
  // one buried in the floor or a wall, where the player then spawns, read as fine: its capsule is what it occupies.
  const ANavigationObjectBase *Start = Cast<ANavigationObjectBase>(Actor);
  const UCapsuleComponent *StartCapsule = Start ? Start->GetCapsuleComponent() : nullptr;
  if ((!StartCapsule && IsBoundsOnlyActor(Actor)) || McpPlacementAccepted(Actor) ||
      McpIsOverlapOnly(Actor) || McpIsWaterBody(Actor)) {
    return;
  }

  McpRefreshBounds(Actor);
  FVector Origin = FVector::ZeroVector;
  FVector Extent = FVector::ZeroVector;
  Actor->GetActorBounds(true, Origin, Extent);
  if (StartCapsule) {
    Origin = StartCapsule->Bounds.Origin;
    Extent = StartCapsule->Bounds.BoxExtent;
  }
  if (!McpHasUsableBounds(Extent)) {
    return;
  }
  const FBox SelfBox = FBox::BuildAABB(Origin, Extent);

  const double IgnoreBelow = McpBoxSample(SelfBox).Graze;
  const bool bSelfSlab = McpIsSlab(Extent);

  TArray<TSharedPtr<FJsonValue>> Overlaps;
  double WorstDepth = 0.0;
  FString WorstName;

  for (TActorIterator<AActor> It(World); It; ++It) {
    AActor *Other = *It;
    if (!Other || Other == Actor || Other->IsHidden() ||
        IsBoundsOnlyActor(Other) || McpPlacementAccepted(Other) || McpIsWaterBody(Other)) {
      continue;
    }
    // An attached child sharing its parent's space is structural, not a mistake.
    if (Other->IsAttachedTo(Actor) || Actor->IsAttachedTo(Other)) {
      continue;
    }
    // A landscape's box runs from its lowest valley to its highest peak, mostly air, and its heightfield cannot be
    // tested shape to shape: a cube resting on the flat part of a hilly landscape read "intersects it by 100", and the
    // landscape "intersected" a ball standing on it. Its surface is what an actor rests on or sinks into, and the
    // ground trace below measures that.
    if (Other->IsA<ALandscapeProxy>() || Actor->IsA<ALandscapeProxy>()) {
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
    if (McpShapesApart(Actor, Other)) {
      continue;
    }

    if (Overlaps.Num() < 8) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("actorName"), McpActorRef(Other));
      Entry->SetStringField(TEXT("actorClass"), Other->GetClass()->GetName());
      Entry->SetNumberField(TEXT("penetrationDepth"), FMath::RoundToDouble(Depth));
      Overlaps.Add(MakeShared<FJsonValueObject>(Entry));
    }
    if (Depth > WorstDepth) {
      WorstDepth = Depth;
      WorstName = McpActorRef(Other);
    }
  }

  // Where the ground is, under each place the actor rests (one box for most actors, every section of a spline mesh): the
  // deepest sample sunk past its floor names the verdict, else the one nearest the ground, which floats only when even it
  // is clear of the ground.
  TArray<FMcpGroundSample> Samples;
  McpGroundSamples(*Actor, SelfBox, Samples);
  bool bHasGround = false;
  bool bSunk = false;
  double GroundZ = 0.0;
  double Clearance = 0.0;
  for (const FMcpGroundSample &Sample : Samples) {
    double SampleGroundZ = 0.0;
    bool bOnWater = false;
    if (!McpGroundUnder(World, Actor, SelfBox.Max.Z, Sample, SampleGroundZ, bOnWater)) {
      continue;
    }
    const double Gap = Sample.Box.Min.Z - SampleGroundZ;
    // Below a water surface is afloat or wading, not sunk; above it still reads floating.
    const bool bDeep = Gap < -Sample.SunkFloor && !bOnWater;
    if (!bHasGround || (bDeep && !bSunk) || (bDeep == bSunk && Gap < Clearance)) {
      bHasGround = true;
      bSunk = bDeep;
      GroundZ = SampleGroundZ;
      Clearance = Gap;
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
    Data->SetNumberField(TEXT("groundZ"), FMath::RoundToDouble(GroundZ));
    Data->SetNumberField(TEXT("groundClearance"), FMath::RoundToDouble(Clearance));

    if (bSunk) {
      // The exact trap that buried a Character: its location is the capsule
      // CENTRE, so reusing a StaticMeshActor's feet-relative Z sinks it by half
      // its height. Hand back the actor location that rests it on the surface: its own location raised by the depth,
      // whatever its pivot (the bounds centre was right for a capsule and moved a corner-pivot mesh sideways).
      const FVector ActorLocation = Actor->GetActorLocation();
      const double SuggestedZ = ActorLocation.Z - Clearance;
      TSharedPtr<FJsonObject> Suggested = MakeShared<FJsonObject>();
      Suggested->SetNumberField(TEXT("x"), ActorLocation.X);
      Suggested->SetNumberField(TEXT("y"), ActorLocation.Y);
      Suggested->SetNumberField(TEXT("z"), FMath::RoundToDouble(SuggestedZ));
      Data->SetObjectField(TEXT("suggestedLocation"), Suggested);
      Notes.Add(FString::Printf(
          TEXT("sunk %.0f units below the surface under it (the bottom of its bounds); z=%.0f would rest it on the "
               "surface"),
          -Clearance, SuggestedZ));
    } else if (Clearance > 50.0) {
      // Only support from below was looked for, so a window band flush on a hall or an awning on a facade
      // read "floating" by the height of the wall it hangs on. Held from the side, it is mounted.
      if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) {
        Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount));
      } else {
        Notes.Add(FString::Printf(
            TEXT("floating %.0f units above the surface under it"), Clearance));
      }
    }
  } else if (AActor *Mount = FindMount(World, Actor, SelfBox, IgnoreBelow)) {
    Data->SetStringField(TEXT("mountedOn"), McpActorRef(Mount));
  } else if (!McpIsGroundLike(Actor)) {
    // A floor or a platform over a pit is what things stand on: nothing under it is how a level is built, and the
    // first floor of every new level was reported "outside the playable area".
    Notes.Add(TEXT("nothing below it - it may be outside the playable area"));
  }

  if (Notes.Num() > 0) {
    Data->SetStringField(
        TEXT("placementWarning"),
        FString::Printf(TEXT("'%s' %s."), *McpActorRef(Actor),
                        *FString::Join(Notes, TEXT("; "))));
  }
}

} // namespace McpPlacement
