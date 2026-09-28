// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace McpPlacementTilt {
/**
 * How far an actor leans off vertical, and how far that displaces its top.
 *
 * Overlap and ground checks both pass for a building lying on its face: it is
 * not inside anything and its (now horizontal) bounds still rest on the floor.
 * That is how eighteen shop houses in one level stood on their gable ends with
 * the sweep reporting nothing -- the +-90 meant to turn them to face the street
 * had been written into pitch instead of yaw.
 *
 * Lean is measured as the angle between the actor's up vector and world up, so
 * yaw -- the rotation that is almost always deliberate -- contributes nothing,
 * and a fully inverted actor reads 180 rather than wrapping back to 0.
 *
 * A rotation is not wrong on its own: a leaning post, a banner, a spotlight all
 * want one. What distinguishes a mistake is how much geometry the angle moves,
 * so severity is the distance the actor's top travelled from upright,
 * 2 * halfHeight * sin(lean/2). That keeps tilt in the same world units as the
 * rest of the sweep -- a toppled house outranks a tipped pebble instead of
 * tying with it at "90" -- and it rises monotonically all the way to inverted.
 */
bool OffVertical(AActor *Actor, double &OutDegrees, double &OutUnits);
} // namespace McpPlacementTilt
