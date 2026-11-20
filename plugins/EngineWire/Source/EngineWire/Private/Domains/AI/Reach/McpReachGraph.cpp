// prove_reachable's graph (McpReachGraph.h): standing points sampled on a grid by downward traces, grouped by what the
// pawn walks between, and the falls and jumps that carry it from one group to another, each jump swept as an arc with
// the pawn's capsule.

#include "Domains/AI/Reach/McpReachGraph.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

namespace McpReachGraphDetail
{
using namespace McpReach;

int32 Root(TArray<int32>& Parent, int32 At)
{
    while (Parent[At] != At)
    {
        Parent[At] = Parent[Parent[At]];
        At = Parent[At];
    }
    return At;
}

FCollisionQueryParams Params(const TArray<const AActor*>& Ignore)
{
    FCollisionQueryParams Out(SCENE_QUERY_STAT(McpProveReachable), false);
    for (const AActor* Actor : Ignore)
    {
        Out.AddIgnoredActor(Actor);
    }
    return Out;
}

// Whether the capsule flies the arc from A to B (feet) without hitting anything before it lands.
bool ArcClear(UWorld* World, const FMover& Mover, const FVector& A, const FVector& B, const FCollisionQueryParams& Query)
{
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Mover.Radius * 0.9, Mover.HalfHeight * 0.9);
    const FVector Up(0.0, 0.0, Mover.HalfHeight + 2.0);
    FVector Last = A + Up;
    const TArray<FVector> Points = Arc(Mover, A, B, 8);
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        // The last leg ends on the landing, where the capsule touches the floor: it stops short of it.
        const FVector Next = Points[Index] + Up + (Index == Points.Num() - 1 ? FVector(0.0, 0.0, 4.0) : FVector::ZeroVector);
        if (World->SweepTestByChannel(Last, Next, FQuat::Identity, ECC_Pawn, Capsule, Query))
        {
            return false;
        }
        Last = Next;
    }
    return true;
}
} // namespace McpReachGraphDetail

namespace McpReach
{
using namespace McpReachGraphDetail;

FGraph Build(UWorld* World, const FMover& Mover, const FBox& Bounds, int32 MaxCells, double DeadlineSeconds,
             const TArray<const AActor*>& Ignore)
{
    FGraph Graph;
    const FVector Size = Bounds.GetSize();
    Graph.Cell = FMath::Max(Mover.Radius * 1.5, FMath::Sqrt(Size.X * Size.Y / FMath::Max(MaxCells, 1)));
    const int32 Nx = FMath::Max(1, FMath::CeilToInt(Size.X / Graph.Cell));
    const int32 Ny = FMath::Max(1, FMath::CeilToInt(Size.Y / Graph.Cell));
    const FCollisionQueryParams Query = Params(Ignore);
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Mover.Radius, Mover.HalfHeight);
    TArray<int32> At;
    At.Init(INDEX_NONE, Nx * Ny);
    for (int32 Y = 0; Y < Ny; ++Y)
    {
        for (int32 X = 0; X < Nx; ++X)
        {
            const FVector Column(Bounds.Min.X + (X + 0.5) * Graph.Cell, Bounds.Min.Y + (Y + 0.5) * Graph.Cell, 0.0);
            FHitResult Hit;
            const bool bHit = World->LineTraceSingleByChannel(Hit, FVector(Column.X, Column.Y, Bounds.Max.Z),
                                                              FVector(Column.X, Column.Y, Bounds.Min.Z), ECC_Pawn, Query);
            // The top surface of the column, when the pawn can stand on it and fits above it.
            if (bHit && Hit.ImpactNormal.Z >= Mover.WalkableZ &&
                !World->OverlapBlockingTestByChannel(Hit.ImpactPoint + FVector(0.0, 0.0, Mover.HalfHeight + 3.0), FQuat::Identity, ECC_Pawn, Capsule, Query))
            {
                At[Y * Nx + X] = Graph.Feet.Add(Hit.ImpactPoint);
            }
        }
    }
    // Walks: neighbours a step apart in height are one group.
    TArray<int32> Parent;
    for (int32 Node = 0; Node < Graph.Feet.Num(); ++Node) Parent.Add(Node);
    const auto Node = [&](int32 X, int32 Y) { return X >= 0 && Y >= 0 && X < Nx && Y < Ny ? At[Y * Nx + X] : INDEX_NONE; };
    TArray<int32> Ledges;
    for (int32 Y = 0; Y < Ny; ++Y)
    {
        for (int32 X = 0; X < Nx; ++X)
        {
            const int32 A = Node(X, Y);
            bool bLedge = false;
            for (int32 Dy = -1; A != INDEX_NONE && Dy <= 1; ++Dy)
            {
                for (int32 Dx = -1; Dx <= 1; ++Dx)
                {
                    const int32 B = (Dx || Dy) ? Node(X + Dx, Y + Dy) : A;
                    const bool bWalk = B != INDEX_NONE && FMath::Abs(Graph.Feet[B].Z - Graph.Feet[A].Z) <= Mover.Step + 1.0;
                    bLedge |= !bWalk;
                    if (bWalk && B != A) Parent[Root(Parent, A)] = Root(Parent, B);
                }
            }
            if (bLedge) Ledges.Add(A);
        }
    }
    TMap<int32, int32> GroupOf;
    for (int32 Each = 0; Each < Graph.Feet.Num(); ++Each)
    {
        const int32 R = Root(Parent, Each);
        const int32* Known = GroupOf.Find(R);
        Graph.Group.Add(Known ? *Known : GroupOf.Add(R, Graph.GroupCount++));
    }
    Graph.Out.SetNum(Graph.GroupCount);
    // Falls and jumps from each ledge to the surfaces of other groups within reach of a fall from twice the apex.
    const double Radius = Reach(Mover, -2.0 * Apex(Mover) - 200.0);
    const int32 Span = FMath::CeilToInt(Radius / Graph.Cell);
    for (const int32 A : Ledges)
    {
        if (FPlatformTime::Seconds() > DeadlineSeconds)
        {
            Graph.bBudget = true;
            break;
        }
        const FVector From = Graph.Feet[A];
        const int32 Ax = FMath::FloorToInt((From.X - Bounds.Min.X) / Graph.Cell);
        const int32 Ay = FMath::FloorToInt((From.Y - Bounds.Min.Y) / Graph.Cell);
        TMap<int32, FJump>& Out = Graph.Out[Graph.Group[A]];
        for (int32 Y = Ay - Span; Y <= Ay + Span; ++Y)
        {
            for (int32 X = Ax - Span; X <= Ax + Span; ++X)
            {
                const int32 B = Node(X, Y);
                if (B == INDEX_NONE || Graph.Group[B] == Graph.Group[A] || Out.Contains(Graph.Group[B]))
                {
                    continue;
                }
                const FVector To = Graph.Feet[B];
                if (FVector::Dist2D(From, To) <= Reach(Mover, To.Z - From.Z) && ArcClear(World, Mover, From, To, Query))
                {
                    Out.Add(Graph.Group[B], {A, B});
                }
            }
        }
    }
    return Graph;
}

int32 NearestNode(const FGraph& Graph, const FVector& Point, double HalfHeight)
{
    int32 Best = INDEX_NONE;
    double BestDistance = TNumericLimits<double>::Max();
    for (int32 Node = 0; Node < Graph.Feet.Num(); ++Node)
    {
        const double Flat = FVector::Dist2D(Graph.Feet[Node], Point);
        const double Rise = FMath::Abs(Graph.Feet[Node].Z - Point.Z);
        if (Flat <= Graph.Cell * 1.5 && Rise <= HalfHeight * 2.5 && Flat + Rise < BestDistance)
        {
            Best = Node;
            BestDistance = Flat + Rise;
        }
    }
    return Best;
}
} // namespace McpReach
