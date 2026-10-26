// Doors, loops and route shares of the spaces of a sampled nav mesh (analyze_level_layout; see McpLevelLayout.h).
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "Foundation/Solvers/McpBetweenness.h"

namespace McpLevelLayout
{
namespace
{
struct FLayoutDoorSum
{
    FVector Sum = FVector::ZeroVector;
    int32 Crossings = 0;
    TSet<int32> SideA;
    TSet<int32> SideB;
};

// The parts of the space graph (its connected components), for its loop count.
int32 LayoutParts(const FLayout& Layout)
{
    TArray<bool> Seen;
    Seen.Init(false, Layout.Spaces.Num());
    int32 Parts = 0;
    for (int32 Seed = 0; Seed < Layout.Spaces.Num(); ++Seed)
    {
        if (Seen[Seed])
        {
            continue;
        }
        ++Parts;
        TArray<int32> Queue = {Seed};
        Seen[Seed] = true;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            for (const int32 Next : Layout.Spaces[Queue[Head]].Neighbours)
            {
                if (!Seen[Next])
                {
                    Seen[Next] = true;
                    Queue.Add(Next);
                }
            }
        }
    }
    return Parts;
}
} // namespace

void AddDoorsAndShares(const FGraph& Graph, FLayout& Layout)
{
    TMap<FIntPoint, FLayoutDoorSum> Sums; // (lower space, higher space)
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        for (const int32 Next : Graph.Adjacency[Node])
        {
            const int32 A = Layout.SpaceOf[Node];
            const int32 B = Layout.SpaceOf[Next];
            if (Next < Node || A == B)
            {
                continue; // each edge once, and only those between two spaces
            }
            FLayoutDoorSum& Sum = Sums.FindOrAdd(FIntPoint(FMath::Min(A, B), FMath::Max(A, B)));
            Sum.Sum += (Graph.Points[Node] + Graph.Points[Next]) * 0.5;
            ++Sum.Crossings;
            (A < B ? Sum.SideA : Sum.SideB).Add(Node);
            (A < B ? Sum.SideB : Sum.SideA).Add(Next);
        }
    }
    TArray<FIntPoint> Pairs;
    Sums.GetKeys(Pairs);
    Pairs.Sort([](const FIntPoint& L, const FIntPoint& R) { return L.X != R.X ? L.X < R.X : L.Y < R.Y; });
    TArray<McpBetweenness::FEdge> Edges;
    for (const FIntPoint& Pair : Pairs)
    {
        const FLayoutDoorSum& Sum = Sums[Pair];
        FDoor Door;
        Door.A = Pair.X;
        Door.B = Pair.Y;
        Door.Location = Sum.Sum / Sum.Crossings;
        Door.WidthCells = FMath::Min(Sum.SideA.Num(), Sum.SideB.Num()); // the wider side counts the corners' diagonals too
        Layout.Doors.Add(Door);
        Layout.Spaces[Door.A].Neighbours.Add(Door.B);
        Layout.Spaces[Door.B].Neighbours.Add(Door.A);
        Edges.Add({Door.A, Door.B,
                   FVector::Dist(Layout.Spaces[Door.A].Center, Door.Location) + FVector::Dist(Door.Location, Layout.Spaces[Door.B].Center)});
    }
    Layout.Loops = Layout.Doors.Num() - Layout.Spaces.Num() + LayoutParts(Layout);

    TArray<double> Weight;
    for (const FSpace& Space : Layout.Spaces)
    {
        Weight.Add(Space.Nodes.Num());
    }
    const McpBetweenness::FResult Shares = McpBetweenness::Compute(Layout.Spaces.Num(), Edges, Weight);
    for (int32 Space = 0; Space < Layout.Spaces.Num(); ++Space)
    {
        Layout.Spaces[Space].PassShare = Shares.NodeShare[Space];
    }
    for (int32 Door = 0; Door < Layout.Doors.Num(); ++Door)
    {
        Layout.Doors[Door].Share = Shares.EdgeShare[Door];
    }
}
} // namespace McpLevelLayout
