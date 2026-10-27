// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace McpPlacement {
/**
 * Volume-only actors (post-process, trigger, audio, kill-Z) legitimately enclose
 * everything inside them, so reporting those as overlaps would bury the real
 * signal under noise. They are no host to mount on either.
 */
bool IsBoundsOnlyActor(const AActor *Actor);

/**
 * The box of what the game draws of Actor: its visible primitives. The sprite the
 * editor gives an actor with only a scene root is editor-only and far bigger than
 * a small prop: an 84 cm robot read as a 256 cm cube round its pivot.
 */
FBox DrawnBounds(const AActor &Actor);

/** The same parts' box measured in Frame (an actor's own turned frame, say): invalid when nothing is drawn. */
FBox DrawnBoundsIn(const AActor &Actor, const FTransform &Frame);

/**
 * The actor Actor hangs on from the side, or null: a window band flush on a hall,
 * an awning on a facade. The placement check only looked for a surface UNDER an
 * actor, so everything mounted on a wall read "floating" by the height of the
 * wall it hangs on (20 window bands and 96 awnings of one level, 139 and 521
 * units up) and the real findings drowned in it.
 *
 * Mounted means the bounds touch or overlap another actor's vertical face: they
 * meet within Touch (the graze distance the overlap check already ignores), the
 * contact is across X or Y rather than a floor or a roof edge (shallower than it
 * is tall), and shallow for the actor (no deeper than Touch twice, or a quarter
 * of the actor's own size there), so a pole floating inside a big actor's box is
 * not "mounted" on it.
 */
AActor *FindMount(UWorld *World, AActor *Actor, const FBox &ActorBox, double Touch);

/** FindMount's test for one pair: whether the actor bounded by ActorBox hangs on OtherBox from the side. */
bool HangsOn(const FBox &ActorBox, const FBox &OtherBox, double Touch);

/** Recomputes the bounds of Actor's parts, which a part reshaped this frame keeps stale until the frame ends. */
void McpRefreshBounds(AActor *Actor);

/**
 * A slab: far wider than it is thick. Floors, aprons, inlays, emblems, road decals and platform tiers are all built this
 * way, and a level assembles them by STACKING them into each other.
 */
bool McpIsSlab(const FVector &Extent);

/**
 * One place the ground under an actor is traced: the bottom of Box is the actor's underside there and its centre the
 * line traced. Graze is the depth a touch may have; SunkFloor how deep it may be bedded before it reads sunk.
 */
struct FMcpGroundSample {
  FBox Box = FBox(ForceInit);
  double Graze = 4.0;
  double SunkFloor = 4.0;
};

/** The sample for a part bounded by Box: a slab may be bedded to its own thickness, anything else to its graze. */
FMcpGroundSample McpBoxSample(const FBox &Box);

/**
 * Where the ground under Actor is judged: SelfBox, the box round its colliding parts, except that each colliding spline
 * mesh is judged at its two end sections and halfway between them, and the box then holds only the other parts.
 */
void McpGroundSamples(const AActor &Actor, const FBox &SelfBox, TArray<FMcpGroundSample> &OutSamples);
} // namespace McpPlacement
