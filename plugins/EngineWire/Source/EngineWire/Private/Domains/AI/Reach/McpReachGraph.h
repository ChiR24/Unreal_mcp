#pragma once

#include "Domains/AI/Reach/McpReach.h"

class AActor;
class UWorld;

// The surfaces a pawn stands on and the moves between them (McpReachGraph.cpp), for prove_reachable.
namespace McpReach
{
struct FJump
{
    int32 From = INDEX_NONE; // nodes
    int32 To = INDEX_NONE;
};

struct FGraph
{
    TArray<FVector> Feet;           // per node: where the feet stand
    TArray<int32> Group;            // per node: the walk-connected group it is in
    int32 GroupCount = 0;
    TArray<TMap<int32, FJump>> Out; // per group: the groups a fall or jump reaches, by the move that does
    double Cell = 0.0;
    bool bBudget = false;           // the time ran out before every jump was tried
};

/** Samples Bounds on a grid (cells at most MaxCells) and links what walks, falls and jumps between its surfaces. */
FGraph Build(UWorld* World, const FMover& Mover, const FBox& Bounds, int32 MaxCells, double DeadlineSeconds,
             const TArray<const AActor*>& Ignore);

/** The node nearest Point that stands within a cell and a capsule height of it; INDEX_NONE when none does. */
int32 NearestNode(const FGraph& Graph, const FVector& Point, double HalfHeight);
} // namespace McpReach
