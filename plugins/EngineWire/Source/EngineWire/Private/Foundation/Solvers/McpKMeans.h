// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// k-means (Lloyd's rounds) over weighted points in three dimensions (Foundation/Solvers), seeded k-means++ style from a
// fixed seed so the same input gives the same clusters every run.
namespace McpKMeans
{
struct FCluster
{
    FVector3f Center = FVector3f::ZeroVector;
    double Weight = 0.0; // the summed weight of its points
};

/** At most K clusters of Points (Weights, when given, one per point), after at most Rounds rounds or once nothing moves;
 *  empty clusters are dropped and the rest sorted heaviest first. */
TArray<FCluster> Cluster(const TArray<FVector3f>& Points, const TArray<float>* Weights, int32 K, int32 Rounds, int32 Seed);
} // namespace McpKMeans
