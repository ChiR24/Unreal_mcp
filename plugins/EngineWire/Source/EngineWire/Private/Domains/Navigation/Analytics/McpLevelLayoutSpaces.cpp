// Spaces, doors and route shares of a sampled nav mesh (analyze_level_layout; see McpLevelLayout.h).
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "Foundation/Solvers/McpWatershed.h"

namespace McpLevelLayout
{
namespace
{
constexpr int32 LayoutMinSpace = 4;       // a space of fewer samples is a fragment and joins a neighbour
constexpr double LayoutMergeRatio = 0.75; // McpWatershed: a saddle this high (of the lower peak) is one room, not a doorway

// A sample is at an edge (a wall, a drop) when it walks to no sample in one of the four cells beside it. Only the sides
// count: a corner a doorway cuts leaves the cell inside it without its diagonal, and reading that cell as an edge took
// the wall beside every doorway out of its room.
bool LayoutAtEdge(const FGraph& Graph, int32 Node)
{
    for (const FIntPoint Side : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
    {
        if (!Graph.Adjacency[Node].ContainsByPredicate([&](int32 Next) { return Graph.Cells[Next] == Graph.Cells[Node] + Side; }))
        {
            return true;
        }
    }
    return false;
}

// Hops from every sample to the nearest edge sample (every part of the graph has one: its last cell has none beyond).
TArray<int32> LayoutHeights(const FGraph& Graph)
{
    TArray<int32> Height;
    Height.Init(INDEX_NONE, Graph.Points.Num());
    TArray<int32> Queue;
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        if (LayoutAtEdge(Graph, Node))
        {
            Height[Node] = 0;
            Queue.Add(Node);
        }
    }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        for (const int32 Next : Graph.Adjacency[Queue[Head]])
        {
            if (Height[Next] == INDEX_NONE)
            {
                Height[Next] = Height[Queue[Head]] + 1;
                Queue.Add(Next);
            }
        }
    }
    return Height;
}

// Whether each sample lies within the inscribed disk of a sample at least RoomHeight from an edge: the reach left after
// walking out from such a sample, passed on from the deepest samples outward.
TArray<bool> LayoutRoomCover(const FGraph& Graph, const TArray<int32>& Height, int32 RoomHeight)
{
    int32 Top = 0;
    for (const int32 Value : Height)
    {
        Top = FMath::Max(Top, Value);
    }
    TArray<int32> Reach;
    Reach.Init(-1, Height.Num());
    TArray<TArray<int32>> Bucket;
    Bucket.SetNum(Top + 1);
    for (int32 Node = 0; Node < Height.Num(); ++Node)
    {
        if (Height[Node] >= RoomHeight)
        {
            Reach[Node] = Height[Node];
            Bucket[Height[Node]].Add(Node);
        }
    }
    for (int32 Level = Top; Level >= 1; --Level)
    {
        for (int32 Index = 0; Index < Bucket[Level].Num(); ++Index)
        {
            const int32 Node = Bucket[Level][Index];
            if (Reach[Node] != Level)
            {
                continue; // reached again from deeper since it was queued
            }
            for (const int32 Next : Graph.Adjacency[Node])
            {
                if (Reach[Next] < Level - 1)
                {
                    Reach[Next] = Level - 1;
                    Bucket[Level - 1].Add(Next);
                }
            }
        }
    }
    TArray<bool> Room;
    for (const int32 Left : Reach)
    {
        Room.Add(Left >= 0);
    }
    return Room;
}

// Labels: rooms from the watershed of the covered samples, then corridors as the connected rest. OutRooms: room labels.
TArray<int32> LayoutLabels(const FGraph& Graph, const TArray<int32>& Height, const TArray<bool>& Room, int32& OutCount,
                           int32& OutRooms)
{
    const int32 Num = Graph.Points.Num();
    TArray<int32> Local;
    Local.Init(INDEX_NONE, Num);
    TArray<int32> Global;
    for (int32 Node = 0; Node < Num; ++Node)
    {
        Local[Node] = Room[Node] ? Global.Add(Node) : INDEX_NONE;
    }
    TArray<TArray<int32>> Adjacency;
    Adjacency.SetNum(Global.Num());
    TArray<int32> Heights;
    for (int32 Index = 0; Index < Global.Num(); ++Index)
    {
        Heights.Add(Height[Global[Index]]);
        for (const int32 Next : Graph.Adjacency[Global[Index]])
        {
            if (Local[Next] != INDEX_NONE)
            {
                Adjacency[Index].Add(Local[Next]);
            }
        }
    }
    TArray<int32> Label;
    Label.Init(INDEX_NONE, Num);
    const TArray<int32> Basin = McpWatershed::Basins(Adjacency, Heights, LayoutMergeRatio, OutCount);
    for (int32 Index = 0; Index < Global.Num(); ++Index)
    {
        Label[Global[Index]] = Basin[Index];
    }
    OutRooms = OutCount;
    for (int32 Seed = 0; Seed < Num; ++Seed)
    {
        if (Label[Seed] != INDEX_NONE)
        {
            continue;
        }
        TArray<int32> Queue = {Seed};
        Label[Seed] = OutCount;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            for (const int32 Next : Graph.Adjacency[Queue[Head]])
            {
                if (Label[Next] == INDEX_NONE)
                {
                    Label[Next] = OutCount;
                    Queue.Add(Next);
                }
            }
        }
        ++OutCount;
    }
    return Label;
}

// A space of fewer than LayoutMinSpace samples joins the neighbouring space it shares the most edges with.
void LayoutAbsorbFragments(const FGraph& Graph, TArray<int32>& Label, int32 Count)
{
    TArray<TArray<int32>> Members;
    Members.SetNum(Count);
    for (int32 Node = 0; Node < Label.Num(); ++Node)
    {
        Members[Label[Node]].Add(Node);
    }
    for (int32 Space = 0; Space < Count; ++Space)
    {
        if (Members[Space].Num() == 0 || Members[Space].Num() >= LayoutMinSpace)
        {
            continue;
        }
        TMap<int32, int32> Shared;
        for (const int32 Node : Members[Space])
        {
            for (const int32 Next : Graph.Adjacency[Node])
            {
                if (Label[Next] != Space)
                {
                    ++Shared.FindOrAdd(Label[Next]);
                }
            }
        }
        int32 Into = INDEX_NONE;
        for (const TPair<int32, int32>& Pair : Shared)
        {
            if (Into == INDEX_NONE || Pair.Value > Shared[Into] || (Pair.Value == Shared[Into] && Pair.Key < Into))
            {
                Into = Pair.Key;
            }
        }
        if (Into == INDEX_NONE)
        {
            continue; // an island of its own: it stays a (small) space
        }
        for (const int32 Node : Members[Space])
        {
            Label[Node] = Into;
        }
        Members[Into].Append(Members[Space]);
        Members[Space].Reset();
    }
}
} // namespace

FLayout Spaces(const FGraph& Graph, int32 RoomCells)
{
    FLayout Out;
    const TArray<int32> Height = LayoutHeights(Graph);
    int32 Count = 0;
    int32 Rooms = 0;
    TArray<int32> Label = LayoutLabels(Graph, Height, LayoutRoomCover(Graph, Height, FMath::Max(1, RoomCells / 2)), Count, Rooms);
    LayoutAbsorbFragments(Graph, Label, Count);

    // Spaces in reading order: rooms first, each kind largest first.
    TArray<FSpace> Raw;
    Raw.SetNum(Count);
    for (int32 Node = 0; Node < Label.Num(); ++Node)
    {
        FSpace& Space = Raw[Label[Node]];
        Space.bRoom = Label[Node] < Rooms;
        Space.Nodes.Add(Node);
        Space.Floor += Graph.Floor[Node];
        Space.Center += Graph.Points[Node];
        if (Space.Deepest == INDEX_NONE || Height[Node] > Space.Height)
        {
            Space.Deepest = Node;
            Space.Height = Height[Node];
        }
    }
    TArray<int32> Order;
    for (int32 Space = 0; Space < Count; ++Space)
    {
        if (Raw[Space].Nodes.Num() > 0)
        {
            Raw[Space].Center /= Raw[Space].Nodes.Num();
            Order.Add(Space);
        }
    }
    Order.Sort([&Raw](int32 L, int32 R)
               {
                   if (Raw[L].bRoom != Raw[R].bRoom) return Raw[L].bRoom;
                   if (Raw[L].Nodes.Num() != Raw[R].Nodes.Num()) return Raw[L].Nodes.Num() > Raw[R].Nodes.Num();
                   return Raw[L].Nodes[0] < Raw[R].Nodes[0];
               });
    TArray<int32> Final;
    Final.Init(INDEX_NONE, Count);
    for (const int32 Space : Order)
    {
        Final[Space] = Out.Spaces.Add(MoveTemp(Raw[Space]));
    }
    for (const int32 Space : Label)
    {
        Out.SpaceOf.Add(Final[Space]);
    }
    AddDoorsAndShares(Graph, Out);
    return Out;
}
} // namespace McpLevelLayout
