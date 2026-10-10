// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;

namespace McpPlacement {
/** Blocks world static, world dynamic or pawns: a trigger or pickup volume cannot interpenetrate anything. */
bool McpBlocksSolids(const UPrimitiveComponent *Comp);

/** Has a body in the physics scene; an instanced mesh keeps one per instance. */
bool McpHasBody(const UPrimitiveComponent *Comp);

/**
 * True when every colliding component pair of the two actors was checked on its
 * real collision and none touch. A pair that cannot be checked keeps the
 * bounding-box verdict, so this returns false for it.
 */
bool McpShapesApart(AActor *Actor, AActor *Other);
} // namespace McpPlacement
