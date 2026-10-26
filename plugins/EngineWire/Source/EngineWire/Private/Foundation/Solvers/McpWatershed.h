// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Watershed on a graph (Foundation/Solvers): the basins of Height (higher is deeper inside), flooded from their peaks,
// nodes visited highest first and ties by index, so the same input always gives the same basins. Where two basins meet,
// the meeting node is a saddle: they merge when it stands at least MergeRatio of the lower basin's peak, so a bump in a
// wide room makes no second room while a doorway (a low saddle between two high peaks) keeps the rooms apart.
namespace McpWatershed
{
/** Each node's basin, 0 to OutCount - 1. Adjacency lists every node's neighbours by index; Height has one entry per node.
 *  A node whose neighbours are all unvisited starts a basin; one between basins that do not merge joins the higher. */
TArray<int32> Basins(const TArray<TArray<int32>>& Adjacency, const TArray<int32>& Height, double MergeRatio,
                     int32& OutCount);
} // namespace McpWatershed
