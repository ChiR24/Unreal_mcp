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
/**
 * How far Actor, moved by Offset, must move to leave Other (OutPush; zero when apart), measured on every colliding
 * component pair with each side's shape (a mesh's bounds box, or a box, sphere or capsule component's own shape)
 * against the other's body, the shorter of the two pushes kept and the deepest pair winning. False when no pair could
 * be measured (instanced or skeletal bodies, no collision): then nothing is known about the pair. InOutCalls counts the
 * engine queries made.
 */
bool McpMeasurePush(AActor *Actor, AActor *Other, const FVector &Offset, FVector &OutPush, int32 &InOutCalls);
} // namespace McpPlacement
