// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpKMeans.h"

#include "Math/RandomStream.h"

namespace McpKMeans
{
TArray<FCluster> Cluster(const TArray<FVector3f>& Points, const TArray<float>* Weights, int32 K, int32 Rounds, int32 Seed)
{
    const int32 Count = Points.Num();
    const auto WeightOf = [Weights](int32 Index) { return Weights ? double((*Weights)[Index]) : 1.0; };
    TArray<FCluster> Clusters;
    if (Count == 0 || K <= 0)
    {
        return Clusters;
    }
    // k-means++: each further centre is drawn with a chance growing with the squared distance to the nearest so far.
    FRandomStream Random(Seed);
    Clusters.Add({Points[Random.RandRange(0, Count - 1)], 0.0});
    TArray<double> Nearest;
    Nearest.Init(TNumericLimits<double>::Max(), Count);
    while (Clusters.Num() < FMath::Min(K, Count))
    {
        double Total = 0.0;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Nearest[Index] = FMath::Min(Nearest[Index], double(FVector3f::DistSquared(Points[Index], Clusters.Last().Center)));
            Total += Nearest[Index] * WeightOf(Index);
        }
        if (Total <= 0.0)
        {
            break; // every point sits on a centre already
        }
        double Pick = Random.GetFraction() * Total;
        int32 Chosen = Count - 1;
        for (int32 Index = 0; Index < Count && Pick >= 0.0; ++Index)
        {
            Pick -= Nearest[Index] * WeightOf(Index);
            Chosen = Index;
        }
        Clusters.Add({Points[Chosen], 0.0});
    }
    TArray<int32> Owner;
    Owner.Init(INDEX_NONE, Count);
    for (int32 Round = 0; Round < Rounds; ++Round)
    {
        bool bMoved = false;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            int32 Best = 0;
            for (int32 Centre = 1; Centre < Clusters.Num(); ++Centre)
            {
                Best = FVector3f::DistSquared(Points[Index], Clusters[Centre].Center) < FVector3f::DistSquared(Points[Index], Clusters[Best].Center) ? Centre : Best;
            }
            bMoved |= Owner[Index] != Best;
            Owner[Index] = Best;
        }
        TArray<FVector3d> Sums;
        Sums.Init(FVector3d::ZeroVector, Clusters.Num());
        for (FCluster& Each : Clusters)
        {
            Each.Weight = 0.0;
        }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Sums[Owner[Index]] += FVector3d(Points[Index]) * WeightOf(Index);
            Clusters[Owner[Index]].Weight += WeightOf(Index);
        }
        for (int32 Centre = 0; Centre < Clusters.Num(); ++Centre)
        {
            if (Clusters[Centre].Weight > 0.0)
            {
                Clusters[Centre].Center = FVector3f(Sums[Centre] / Clusters[Centre].Weight);
            }
        }
        if (!bMoved)
        {
            break;
        }
    }
    Clusters.RemoveAll([](const FCluster& Each) { return Each.Weight <= 0.0; });
    Clusters.Sort([](const FCluster& A, const FCluster& B) { return A.Weight > B.Weight; });
    return Clusters;
}
} // namespace McpKMeans
