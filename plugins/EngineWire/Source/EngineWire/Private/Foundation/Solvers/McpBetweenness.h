// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Betweenness on a small weighted graph (Foundation/Solvers): Brandes' accumulation over Dijkstra from every node, each
// node standing for Weight points (a room's floor samples). A share is the part of the routes between points of
// different nodes that passes through a node, or along an edge; tied shortest routes split it evenly, and only pairs a
// route joins count (a part cut off from the rest lowers no share). Exact, and meant for tens to hundreds of nodes
// (Dijkstra is the simple quadratic one).
namespace McpBetweenness
{
struct FEdge
{
    int32 A = 0;
    int32 B = 0;
    double Length = 1.0;
};

struct FResult
{
    /** Routes through each node (neither starting nor ending in it), of the routes between the other nodes' points. */
    TArray<double> NodeShare;
    /** Routes along each edge, of the routes between points of different nodes. */
    TArray<double> EdgeShare;
};

FResult Compute(int32 NumNodes, const TArray<FEdge>& Edges, const TArray<double>& Weight);
} // namespace McpBetweenness
