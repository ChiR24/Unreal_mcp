// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// The straight skeleton of a simple polygon, the shape a hip roof takes over it: every edge moves inward at unit speed
// and each vertex rides the two moving lines of its edges. Edge events (two neighbours meet) and split events (a reflex
// vertex reaches another edge) are found by brute force at every step, the earliest applied, so nothing goes stale;
// ties (a square, an L with equal arms) are broken by a deterministic nudge of the outline (1e-3 cm) and the skeleton's
// nodes welded within 0.05 cm afterwards. A node's time is its distance from the outline, the height of the roof there
// once multiplied by the pitch's tangent.
namespace McpRoofSkeleton
{
struct FNode
{
    FVector2D Point = FVector2D::ZeroVector;
    double Time = 0.0;
};

struct FResult
{
    TArray<FVector2D> Outline;      // counter-clockwise, collinear corners dropped: Nodes[i] is Outline[i]
    TArray<FNode> Nodes;            // welded
    TArray<TArray<int32>> Faces;    // one per outline edge i: Nodes i, i + 1, then the skeleton back (counter-clockwise)
    FString Error;                  // set when the outline cannot carry a roof
};

/** The skeleton of Outline (3 to 64 corners, either winding, no holes). */
FResult Build(const TArray<FVector2D>& Outline);

/** Outline moved outward by Distance (each edge along its normal, corners mitred), counter-clockwise. */
TArray<FVector2D> Offset(const TArray<FVector2D>& Outline, double Distance);

/** True when two edges of the closed Outline cross or touch away from their shared corner. */
bool CrossesItself(const TArray<FVector2D>& Outline);
} // namespace McpRoofSkeleton
