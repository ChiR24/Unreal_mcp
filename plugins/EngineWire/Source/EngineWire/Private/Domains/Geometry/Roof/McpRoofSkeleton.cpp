// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Geometry/Roof/McpRoofSkeleton.h"

#include "Domains/Geometry/Roof/McpRoofOutline.h"

namespace McpRoofSkeleton
{
namespace
{
constexpr double RoofEps = 1e-9;
constexpr double RoofJitter = 1e-3; // cm: breaks ties between simultaneous events
constexpr double RoofWeld = 0.05;   // cm: nodes this close are one
constexpr int32 RoofMaxSteps = 10000;

// A wavefront vertex: where it was at T0, how it moves, the outline edges it rides and the node it left from.
struct FRoofWave
{
    FVector2D P0 = FVector2D::ZeroVector;
    double T0 = 0.0;
    FVector2D V = FVector2D::ZeroVector;
    bool bMoves = false; // false between parallel edges
    int32 In = 0;
    int32 Out = 0;
    int32 Node = 0;
};

// A path a wavefront vertex traced, between the faces of its two edges.
struct FRoofArc
{
    int32 A = 0;
    int32 B = 0;
    int32 EdgeIn = 0;
    int32 EdgeOut = 0;
};

struct FRoofEvent
{
    double T = TNumericLimits<double>::Max();
    bool bSplit = false;
    int32 Lav = INDEX_NONE;
    int32 K = 0;
    int32 J = 0;
};

struct FRoofSim
{
    TArray<FVector2D> Dir;
    TArray<FVector2D> Normal; // inward
    TArray<double> Line;      // edge i at time t: Normal[i] . x = Line[i] + t
    TArray<FNode> Nodes;
    TArray<FRoofArc> Arcs;
    TArray<TArray<FRoofWave>> Lavs;
    double Now = 0.0;

    FVector2D At(const FRoofWave& W, double T) const { return W.P0 + W.V * (T - W.T0); }
    int32 AddNode(const FVector2D& P, double T) { return Nodes.Add({P, T}); }
    bool Reflex(const FRoofWave& W) const { return Dir[W.In].X * Dir[W.Out].Y - Dir[W.In].Y * Dir[W.Out].X < -1e-12; }

    FRoofWave Make(const FVector2D& P, double T, int32 In, int32 Out, int32 Node) const
    {
        FRoofWave W;
        W.P0 = P;
        W.T0 = T;
        W.In = In;
        W.Out = Out;
        W.Node = Node;
        const double Det = Normal[In].X * Normal[Out].Y - Normal[In].Y * Normal[Out].X;
        W.bMoves = FMath::Abs(Det) > 1e-12;
        W.V = W.bMoves ? FVector2D((Normal[Out].Y - Normal[In].Y) / Det, (Normal[In].X - Normal[Out].X) / Det) : FVector2D::ZeroVector;
        return W;
    }

    // The earliest event: two neighbours meeting on their shared edge, or a reflex vertex reaching an edge of its chain.
    FRoofEvent Next() const
    {
        FRoofEvent Best;
        for (int32 L = 0; L < Lavs.Num(); ++L)
        {
            const TArray<FRoofWave>& Lav = Lavs[L];
            const int32 M = Lav.Num();
            for (int32 K = 0; K < M; ++K)
            {
                const FRoofWave& U = Lav[K];
                const FRoofWave& W = Lav[(K + 1) % M];
                const double B = FVector2D::DotProduct(W.V - U.V, Dir[U.Out]);
                const double A = FVector2D::DotProduct((W.P0 - W.V * W.T0) - (U.P0 - U.V * U.T0), Dir[U.Out]);
                if (U.bMoves && W.bMoves && B < -RoofEps && -A / B >= Now - 1e-9 && -A / B < Best.T)
                {
                    Best = {-A / B, false, L, K, 0};
                }
            }
            for (int32 K = 0; K < M; ++K)
            {
                const FRoofWave& R = Lav[K];
                for (int32 J = 0; R.bMoves && Reflex(R) && J < M; ++J)
                {
                    const FRoofWave& U = Lav[J];
                    const FRoofWave& W = Lav[(J + 1) % M];
                    const int32 Edge = U.Out;
                    const double B = FVector2D::DotProduct(Normal[Edge], R.V) - 1.0;
                    if (J == K || (J + 1) % M == K || !U.bMoves || !W.bMoves || B > -RoofEps)
                    {
                        continue;
                    }
                    const double T = -(FVector2D::DotProduct(Normal[Edge], R.P0 - R.V * R.T0) - Line[Edge]) / B;
                    const double Along = FVector2D::DotProduct(At(R, T) - At(U, T), Dir[Edge]);
                    const double Length = FVector2D::DotProduct(At(W, T) - At(U, T), Dir[Edge]);
                    if (T >= Now - 1e-9 && T < Best.T && Length > 1e-9 && Along > 1e-9 && Along < Length - 1e-9)
                    {
                        Best = {T, true, L, K, J};
                    }
                }
            }
        }
        return Best;
    }

    void ApplyEdge(const FRoofEvent& Event)
    {
        TArray<FRoofWave>& Lav = Lavs[Event.Lav];
        const int32 M = Lav.Num();
        const FRoofWave U = Lav[Event.K];
        const FRoofWave W = Lav[(Event.K + 1) % M];
        const FVector2D P = At(U, Now);
        const int32 N = AddNode(P, Now);
        Arcs.Add({U.Node, N, U.In, U.Out});
        Arcs.Add({W.Node, N, W.In, W.Out});
        Lav[Event.K] = Make(P, Now, U.In, W.Out, N);
        Lav.RemoveAt(Event.K + 1 < M ? Event.K + 1 : 0);
    }

    // The chain splits in two where a reflex vertex meets an edge: one new vertex on each side of the hit point.
    void ApplySplit(const FRoofEvent& Event)
    {
        const TArray<FRoofWave> Lav = Lavs[Event.Lav];
        const int32 M = Lav.Num();
        const FRoofWave& R = Lav[Event.K];
        const int32 Edge = Lav[Event.J].Out;
        const FVector2D P = At(R, Now);
        const int32 N = AddNode(P, Now);
        Arcs.Add({R.Node, N, R.In, R.Out});
        TArray<FRoofWave> First = {Make(P, Now, R.In, Edge, N)};
        for (int32 I = (Event.J + 1) % M; I != Event.K; I = (I + 1) % M)
        {
            First.Add(Lav[I]);
        }
        TArray<FRoofWave> Second = {Make(P, Now, Edge, R.Out, N)};
        for (int32 I = (Event.K + 1) % M; I != (Event.J + 1) % M; I = (I + 1) % M)
        {
            Second.Add(Lav[I]);
        }
        Lavs.RemoveAt(Event.Lav);
        Lavs.Add(MoveTemp(First));
        Lavs.Add(MoveTemp(Second));
    }

    // A chain of two vertices has no area left: its vertices are joined by a ridge.
    void CloseTiny()
    {
        for (int32 L = Lavs.Num() - 1; L >= 0; --L)
        {
            const TArray<FRoofWave> Lav = Lavs[L];
            if (Lav.Num() > 2)
            {
                continue;
            }
            if (Lav.Num() == 2)
            {
                const int32 N = AddNode(At(Lav[1], Now), Now);
                Arcs.Add({Lav[1].Node, N, Lav[1].In, Lav[1].Out});
                Arcs.Add({N, Lav[0].Node, Lav[0].In, Lav[0].Out});
            }
            Lavs.RemoveAt(L);
        }
    }
};
} // namespace

FResult Build(const TArray<FVector2D>& Outline)
{
    FResult Result;
    Result.Outline = McpRoofOutline::Clean(Outline);
    const int32 N = Result.Outline.Num();
    if (N < 3 || CrossesItself(Result.Outline))
    {
        Result.Error = N < 3 ? TEXT("the footprint needs 3 corners or more, apart and not all in a line") : TEXT("the footprint crosses itself");
        return Result;
    }
    FRoofSim Sim;
    TArray<FVector2D> Nudged;
    for (int32 I = 0; I < N; ++I)
    {
        Nudged.Add(Result.Outline[I] + FVector2D(RoofJitter * FMath::Sin(I * 12.9898 + 1.0), RoofJitter * FMath::Cos(I * 78.233 + 2.0)));
    }
    for (int32 I = 0; I < N; ++I)
    {
        Sim.Dir.Add((Nudged[(I + 1) % N] - Nudged[I]).GetSafeNormal());
        Sim.Normal.Add(FVector2D(-Sim.Dir[I].Y, Sim.Dir[I].X));
        Sim.Line.Add(FVector2D::DotProduct(Sim.Normal[I], Nudged[I]));
        Sim.AddNode(Result.Outline[I], 0.0);
    }
    TArray<FRoofWave>& First = Sim.Lavs.AddDefaulted_GetRef();
    for (int32 I = 0; I < N; ++I)
    {
        First.Add(Sim.Make(Nudged[I], 0.0, (I + N - 1) % N, I, I));
    }
    for (int32 Step = 0; Step < RoofMaxSteps && Sim.Lavs.Num() > 0; ++Step)
    {
        const FRoofEvent Event = Sim.Next();
        if (Event.Lav == INDEX_NONE)
        {
            break;
        }
        Sim.Now = Event.T;
        Event.bSplit ? Sim.ApplySplit(Event) : Sim.ApplyEdge(Event);
        Sim.CloseTiny();
    }
    TArray<McpRoofOutline::FArc> Arcs;
    for (const FRoofArc& Arc : Sim.Arcs)
    {
        Arcs.Add({Arc.A, Arc.B, Arc.EdgeIn, Arc.EdgeOut});
    }
    if (Sim.Lavs.Num() > 0 || !McpRoofOutline::Faces(Sim.Nodes, Arcs, N, RoofWeld, Result.Nodes, Result.Faces))
    {
        Result.Error = TEXT("the roof could not be resolved for this footprint");
    }
    return Result;
}

TArray<FVector2D> Offset(const TArray<FVector2D>& Outline, double Distance)
{
    return McpRoofOutline::Offset(McpRoofOutline::Clean(Outline), Distance);
}

bool CrossesItself(const TArray<FVector2D>& Outline)
{
    return McpRoofOutline::CrossesItself(Outline);
}
} // namespace McpRoofSkeleton
