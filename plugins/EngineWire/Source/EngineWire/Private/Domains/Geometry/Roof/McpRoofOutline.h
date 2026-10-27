// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Domains/Geometry/Roof/McpRoofSkeleton.h"

// The outline work around the straight skeleton (McpRoofSkeleton): cleaning a footprint, moving it outward for eaves,
// testing it for self-crossing, and turning the traced arcs into one face per outline edge.
namespace McpRoofOutline
{
/** Counter-clockwise, corners closer than 0.5 cm merged, corners in a straight line dropped. */
TArray<FVector2D> Clean(const TArray<FVector2D>& Outline);

/** A clean counter-clockwise Outline moved outward by Distance, each corner where its two moved edges meet. */
TArray<FVector2D> Offset(const TArray<FVector2D>& Outline, double Distance);

bool CrossesItself(const TArray<FVector2D>& Outline);

struct FArc
{
    int32 A = 0;
    int32 B = 0;
    int32 EdgeIn = 0;
    int32 EdgeOut = 0;
};

/**
 * Nodes within Weld of each other made one, then each outline edge's face: its two corners, then the arcs traced on it
 * back to the first. The first EdgeCount nodes are the outline's corners and keep their indices. False when a face
 * does not close.
 */
bool Faces(const TArray<McpRoofSkeleton::FNode>& Nodes, const TArray<FArc>& Arcs, int32 EdgeCount, double Weld,
           TArray<McpRoofSkeleton::FNode>& OutNodes, TArray<TArray<int32>>& OutFaces);
} // namespace McpRoofOutline
