// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpBetweenness.h"

namespace McpBetweenness
{
FResult Compute(int32 NumNodes, const TArray<FEdge>& Edges, const TArray<double>& Weight)
{
    TArray<TArray<TPair<int32, int32>>> Adjacent; // neighbour, edge
    Adjacent.SetNum(NumNodes);
    for (int32 Edge = 0; Edge < Edges.Num(); ++Edge)
    {
        Adjacent[Edges[Edge].A].Add({Edges[Edge].B, Edge});
        Adjacent[Edges[Edge].B].Add({Edges[Edge].A, Edge});
    }
    TArray<double> Through;
    Through.Init(0.0, NumNodes);
    TArray<double> Along;
    Along.Init(0.0, Edges.Num());
    double AllPairs = 0.0; // ordered pairs of points in different nodes that a route joins
    TArray<double> Pairs;  // the same, per node, for pairs that neither start nor end in it
    Pairs.Init(0.0, NumNodes);
    for (int32 Source = 0; Source < NumNodes; ++Source)
    {
        TArray<double> Dist;
        Dist.Init(TNumericLimits<double>::Max(), NumNodes);
        TArray<double> Sigma; // shortest routes from Source
        Sigma.Init(0.0, NumNodes);
        TArray<TArray<TPair<int32, int32>>> Before; // predecessor, edge
        Before.SetNum(NumNodes);
        TArray<bool> Done;
        Done.Init(false, NumNodes);
        TArray<int32> Settled;
        Dist[Source] = 0.0;
        Sigma[Source] = 1.0;
        for (;;)
        {
            int32 Node = INDEX_NONE;
            for (int32 Candidate = 0; Candidate < NumNodes; ++Candidate)
            {
                if (!Done[Candidate] && Dist[Candidate] < TNumericLimits<double>::Max() &&
                    (Node == INDEX_NONE || Dist[Candidate] < Dist[Node]))
                {
                    Node = Candidate;
                }
            }
            if (Node == INDEX_NONE)
            {
                break;
            }
            Done[Node] = true;
            Settled.Add(Node);
            for (const TPair<int32, int32>& Next : Adjacent[Node])
            {
                const double Via = Dist[Node] + Edges[Next.Value].Length;
                const double Tolerance = 1e-9 * FMath::Max(1.0, Via);
                if (Via < Dist[Next.Key] - Tolerance)
                {
                    Dist[Next.Key] = Via;
                    Sigma[Next.Key] = Sigma[Node];
                    Before[Next.Key] = {{Node, Next.Value}};
                }
                else if (FMath::Abs(Via - Dist[Next.Key]) <= Tolerance && !Done[Next.Key])
                {
                    Sigma[Next.Key] += Sigma[Node];
                    Before[Next.Key].Add({Node, Next.Value});
                }
            }
        }
        TArray<double> Delta; // the routes from Source that run on past each node, weighted by their targets
        Delta.Init(0.0, NumNodes);
        for (int32 Index = Settled.Num() - 1; Index >= 0; --Index)
        {
            const int32 Node = Settled[Index];
            for (const TPair<int32, int32>& Prior : Before[Node])
            {
                const double Part = Sigma[Prior.Key] / Sigma[Node] * (Weight[Node] + Delta[Node]);
                Along[Prior.Value] += Weight[Source] * Part;
                Delta[Prior.Key] += Part;
            }
            if (Node != Source)
            {
                Through[Node] += Weight[Source] * Delta[Node];
            }
        }
        double Reached = 0.0; // points a route from Source reaches
        for (const int32 Node : Settled)
        {
            Reached += Node != Source ? Weight[Node] : 0.0;
        }
        AllPairs += Weight[Source] * Reached;
        for (int32 Node = 0; Node < NumNodes; ++Node)
        {
            Pairs[Node] += Node != Source ? Weight[Source] * (Reached - (Done[Node] ? Weight[Node] : 0.0)) : 0.0;
        }
    }

    FResult Out;
    for (const double Routes : Along)
    {
        Out.EdgeShare.Add(AllPairs > 0.0 ? Routes / AllPairs : 0.0);
    }
    for (int32 Node = 0; Node < NumNodes; ++Node)
    {
        Out.NodeShare.Add(Pairs[Node] > 0.0 ? Through[Node] / Pairs[Node] : 0.0);
    }
    return Out;
}
} // namespace McpBetweenness
