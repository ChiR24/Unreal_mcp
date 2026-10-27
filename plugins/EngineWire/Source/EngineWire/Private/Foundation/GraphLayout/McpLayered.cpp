#include "Foundation/GraphLayout/McpLayered.h"

namespace McpLayered
{
namespace LayeredSteps
{
constexpr float ColumnGap = 96.0f;
constexpr float RowGap = 40.0f;
// A long link passes each column it spans as a point this tall, so the boxes in that column leave it a lane.
constexpr float LaneHeight = 24.0f;

// The links a walk from box 0 (then from every box nothing links to, then from the rest) does not find leading back
// onto itself: a loop wired back is laid out as if that link were not there.
TArray<FEdge> ForwardEdges(int32 Count, const TArray<FEdge>& Edges)
{
    TArray<TArray<int32>> Out;
    Out.SetNum(Count);
    TArray<int32> Incoming;
    Incoming.Init(0, Count);
    for (int32 At = 0; At < Edges.Num(); ++At)
    {
        Out[Edges[At].From].Add(At);
        ++Incoming[Edges[At].To];
    }
    for (TArray<int32>& List : Out)
    {
        List.Sort([&Edges](int32 A, int32 B) { return Edges[A].Order < Edges[B].Order; });
    }
    TArray<int32> Roots = {0};
    TArray<int32> Rest;
    for (int32 Node = 0; Node < Count; ++Node)
    {
        (Incoming[Node] == 0 ? Roots : Rest).Add(Node);
    }
    Roots.Append(Rest);
    TArray<uint8> State; // 0 not reached, 1 on the walk, 2 done
    State.Init(0, Count);
    TArray<bool> Back;
    Back.Init(false, Edges.Num());
    for (const int32 Root : Roots)
    {
        if (State[Root] != 0)
        {
            continue;
        }
        TArray<TPair<int32, int32>> Walk = {TPair<int32, int32>(Root, 0)};
        State[Root] = 1;
        while (Walk.Num() > 0)
        {
            const int32 Node = Walk.Last().Key;
            if (Walk.Last().Value >= Out[Node].Num())
            {
                State[Node] = 2;
                Walk.Pop();
                continue;
            }
            const int32 Edge = Out[Node][Walk.Last().Value++];
            const int32 To = Edges[Edge].To;
            Back[Edge] = State[To] == 1;
            if (State[To] == 0)
            {
                State[To] = 1;
                Walk.Add(TPair<int32, int32>(To, 0));
            }
        }
    }
    TArray<FEdge> Forward;
    for (int32 At = 0; At < Edges.Num(); ++At)
    {
        if (!Back[At])
        {
            Forward.Add(Edges[At]);
        }
    }
    return Forward;
}

// Longest path from the boxes nothing links to; then a pure value moves right to the column before its first reader.
TArray<int32> Columns(int32 Count, const TArray<FEdge>& Forward)
{
    TArray<int32> Incoming;
    Incoming.Init(0, Count);
    TArray<TArray<int32>> Next;
    Next.SetNum(Count);
    TArray<bool> Exec;
    Exec.Init(false, Count);
    for (const FEdge& Edge : Forward)
    {
        ++Incoming[Edge.To];
        Next[Edge.From].Add(Edge.To);
        Exec[Edge.From] |= Edge.bExec;
        Exec[Edge.To] |= Edge.bExec;
    }
    TArray<int32> Order;
    for (int32 Node = 0; Node < Count; ++Node)
    {
        if (Incoming[Node] == 0)
        {
            Order.Add(Node);
        }
    }
    for (int32 Head = 0; Head < Order.Num(); ++Head)
    {
        for (const int32 To : Next[Order[Head]])
        {
            if (--Incoming[To] == 0)
            {
                Order.Add(To);
            }
        }
    }
    TArray<int32> Column;
    Column.Init(0, Count);
    for (const int32 Node : Order)
    {
        for (const int32 To : Next[Node])
        {
            Column[To] = FMath::Max(Column[To], Column[Node] + 1);
        }
    }
    for (int32 At = Order.Num() - 1; At >= 0; --At)
    {
        const int32 Node = Order[At];
        int32 Reader = MAX_int32;
        for (const int32 To : Next[Node])
        {
            Reader = FMath::Min(Reader, Column[To]);
        }
        Column[Node] = Node != 0 && !Exec[Node] && Reader != MAX_int32 ? FMath::Max(Column[Node], Reader - 1) : Column[Node];
    }
    return Column;
}
} // namespace LayeredSteps

TArray<FVector2D> Layout(const TArray<FBox>& Boxes, const TArray<FEdge>& Edges, const FVector2D& Anchor)
{
    using namespace LayeredSteps;
    const int32 Count = Boxes.Num();
    TArray<FVector2D> Out;
    Out.Init(Anchor, Count);
    if (Count == 0)
    {
        return Out;
    }
    const TArray<FEdge> Forward = ForwardEdges(Count, Edges);
    TArray<int32> Column = Columns(Count, Forward);
    TArray<float> Height;
    for (const FBox& Box : Boxes)
    {
        Height.Add(Box.Height);
    }
    // A long link becomes a chain of lane points, one per column it spans.
    TArray<TArray<int32>> Prev;
    Prev.SetNum(Count);
    TArray<TArray<int32>> Next = Prev;
    for (const FEdge& Edge : Forward)
    {
        int32 From = Edge.From;
        for (int32 Lane = Column[Edge.From] + 1; Lane < Column[Edge.To]; ++Lane)
        {
            const int32 Point = Column.Add(Lane);
            Height.Add(LaneHeight);
            Prev.AddDefaulted();
            Next.AddDefaulted();
            Next[From].Add(Point);
            Prev[Point].Add(From);
            From = Point;
        }
        Next[From].Add(Edge.To);
        Prev[Edge.To].Add(From);
    }
    // Columns in the order a walk along the links first reaches each node; then barycentre sweeps, down and up.
    TArray<int32> Reached;
    Reached.Init(MAX_int32, Column.Num());
    TArray<int32> Walk = {0};
    for (int32 Head = 0, Stamp = 0; Head < Walk.Num(); ++Head)
    {
        if (Reached[Walk[Head]] == MAX_int32)
        {
            Reached[Walk[Head]] = Stamp++;
            Walk.Append(Next[Walk[Head]]);
        }
    }
    TArray<TArray<int32>> Lanes;
    for (int32 Node = 0; Node < Column.Num(); ++Node)
    {
        Lanes.SetNum(FMath::Max(Lanes.Num(), Column[Node] + 1));
        Lanes[Column[Node]].Add(Node);
    }
    TArray<float> Rank;
    Rank.Init(0.0f, Column.Num());
    for (TArray<int32>& Lane : Lanes)
    {
        Lane.StableSort([&Reached](int32 A, int32 B) { return Reached[A] < Reached[B]; });
        for (int32 At = 0; At < Lane.Num(); ++At)
        {
            Rank[Lane[At]] = At;
        }
    }
    for (int32 Sweep = 0; Sweep < 4; ++Sweep)
    {
        const bool bDown = Sweep % 2 == 0;
        for (int32 Step = 1; Step < Lanes.Num(); ++Step)
        {
            TArray<int32>& Lane = Lanes[bDown ? Step : Lanes.Num() - 1 - Step];
            const auto Centre = [&](int32 Node)
            {
                const TArray<int32>& Side = bDown ? Prev[Node] : Next[Node];
                float Sum = 0.0f;
                for (const int32 Other : Side)
                {
                    Sum += Rank[Other];
                }
                return Side.Num() > 0 ? Sum / Side.Num() : Rank[Node];
            };
            Lane.StableSort([&Centre](int32 A, int32 B) { return Centre(A) < Centre(B); });
            for (int32 At = 0; At < Lane.Num(); ++At)
            {
                Rank[Lane[At]] = At;
            }
        }
    }
    // Columns left to right; in each, a node wants the middle of what feeds it and is pushed below the one above.
    TArray<float> Top;
    Top.Init(0.0f, Column.Num());
    float Left = 0.0f;
    for (const TArray<int32>& Lane : Lanes)
    {
        float Width = 0.0f;
        float Floor = -MAX_flt;
        for (const int32 Node : Lane)
        {
            float Wanted = Floor == -MAX_flt ? 0.0f : Floor;
            if (Prev[Node].Num() > 0)
            {
                float Sum = 0.0f;
                for (const int32 Other : Prev[Node])
                {
                    Sum += Top[Other] + Height[Other] * 0.5f;
                }
                Wanted = Sum / Prev[Node].Num() - Height[Node] * 0.5f;
            }
            Top[Node] = Floor == -MAX_flt ? Wanted : FMath::Max(Wanted, Floor);
            Floor = Top[Node] + Height[Node] + RowGap;
            Width = Node < Count ? FMath::Max(Width, Boxes[Node].Width) : Width;
            if (Node < Count)
            {
                Out[Node] = FVector2D(Left, Top[Node]);
            }
        }
        Left += Width + ColumnGap;
    }
    const FVector2D Shift = Anchor - Out[0];
    for (FVector2D& Corner : Out)
    {
        Corner += Shift;
    }
    return Out;
}
} // namespace McpLayered
