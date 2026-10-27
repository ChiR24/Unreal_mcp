// Copyright (c) 2024 EngineWire Contributors
//
// Filling, draining and checking a heightfield (McpTerrainErosion.h).

#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

namespace McpTerrainErosion
{
FFill Fill(FGrid& Grid)
{
    const int32 Count = Grid.Num();
    TArray<uint8> Closed, Raised;
    Closed.SetNumZeroed(Count);
    Raised.SetNumZeroed(Count);
    // Lowest first, ties by index, so the order is the same on every run.
    const auto Lower = [&Grid](int32 A, int32 B) { return Grid.Heights[A] < Grid.Heights[B] || (Grid.Heights[A] == Grid.Heights[B] && A < B); };
    TArray<int32> Open, Pit;
    Pit.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Grid.IsBorder(Index))
        {
            Closed[Index] = 1;
            Open.HeapPush(Index, Lower);
        }
    }
    // Priority-Flood + FIFO: a cell at or below the one that reached it is part of that cell's basin, raised to its
    // level and taken next from the plain queue; anything higher waits in the heap.
    FFill Result;
    int32 PitHead = 0;
    while (PitHead < Pit.Num() || Open.Num() > 0)
    {
        int32 Cell = INDEX_NONE;
        if (PitHead < Pit.Num())
        {
            Cell = Pit[PitHead++];
        }
        else
        {
            Open.HeapPop(Cell, Lower);
        }
        const double Level = Grid.Heights[Cell];
        ForEachNeighbour(Grid, Cell, [&](int32 Next, int32)
        {
            if (Closed[Next])
            {
                return;
            }
            Closed[Next] = 1;
            double& Height = Grid.Heights[Next];
            if (Height > Level)
            {
                Open.HeapPush(Next, Lower);
                return;
            }
            if (Height < Level)
            {
                Result.MaxRise = FMath::Max(Result.MaxRise, Level - Height);
                Height = Level;
                Raised[Next] = 1;
                ++Result.Cells;
            }
            Pit.Add(Next);
        });
    }
    // Each connected patch of raised cells is one depression.
    TArray<int32> Queue;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Raised[Index] != 1)
        {
            continue;
        }
        ++Result.Depressions;
        Raised[Index] = 2;
        Queue.Reset();
        Queue.Add(Index);
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            ForEachNeighbour(Grid, Queue[Head], [&](int32 Next, int32)
            {
                if (Raised[Next] == 1)
                {
                    Raised[Next] = 2;
                    Queue.Add(Next);
                }
            });
        }
    }
    return Result;
}

FFlow Flow(const FGrid& Grid)
{
    const int32 Count = Grid.Num();
    FFlow Out;
    Out.Receiver.Init(INDEX_NONE, Count);
    Out.Distance.Init(0.0, Count);
    TArray<uint8> Seen;
    Seen.SetNumZeroed(Count);
    TArray<int32> Queue;
    Queue.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Grid.IsBorder(Index))
        {
            Seen[Index] = 1;
            Queue.Add(Index);
            continue;
        }
        double Steepest = 0.0;
        ForEachNeighbour(Grid, Index, [&](int32 Next, int32 K)
        {
            const double Slope = (Grid.Heights[Index] - Grid.Heights[Next]) / Grid.Step(K);
            if (Slope > Steepest)
            {
                Steepest = Slope;
                Out.Receiver[Index] = Next;
                Out.Distance[Index] = Grid.Step(K);
            }
        });
        if (Out.Receiver[Index] != INDEX_NONE)
        {
            Seen[Index] = 1;
            Queue.Add(Index);
        }
    }
    // A flat has no lower neighbour: each of its cells drains to the one an equal-height search from its outlets (and
    // the border) reached it from. Not the fill's own order: once erosion moves heights, that cell can stand higher.
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 Cell = Queue[Head];
        ForEachNeighbour(Grid, Cell, [&](int32 Next, int32 K)
        {
            if (!Seen[Next] && Grid.Heights[Next] == Grid.Heights[Cell])
            {
                Seen[Next] = 1;
                Out.Receiver[Next] = Cell;
                Out.Distance[Next] = Grid.Step(K);
                Queue.Add(Next);
            }
        });
    }
    Out.Pits = Count - Queue.Num();

    // Donors per cell, then receivers before donors: a breadth-first walk up from every cell that drains nowhere.
    Out.DonorStart.Init(0, Count + 1);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Out.Receiver[Index] != INDEX_NONE)
        {
            ++Out.DonorStart[Out.Receiver[Index] + 1];
        }
    }
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Out.DonorStart[Index + 1] += Out.DonorStart[Index];
    }
    Out.Donors.SetNumUninitialized(Out.DonorStart[Count]);
    TArray<int32> Slot(Out.DonorStart.GetData(), Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Out.Receiver[Index] != INDEX_NONE)
        {
            Out.Donors[Slot[Out.Receiver[Index]]++] = Index;
        }
    }
    Out.Order.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (Out.Receiver[Index] == INDEX_NONE)
        {
            Out.Order.Add(Index);
        }
    }
    for (int32 Head = 0; Head < Out.Order.Num(); ++Head)
    {
        const int32 Cell = Out.Order[Head];
        for (int32 Donor = Out.DonorStart[Cell]; Donor < Out.DonorStart[Cell + 1]; ++Donor)
        {
            Out.Order.Add(Out.Donors[Donor]);
        }
    }
    Out.Area.Init(Grid.CellX * Grid.CellY, Count);
    for (int32 Position = Out.Order.Num() - 1; Position >= 0; --Position)
    {
        const int32 Cell = Out.Order[Position];
        if (Out.Receiver[Cell] != INDEX_NONE)
        {
            Out.Area[Out.Receiver[Cell]] += Out.Area[Cell];
        }
    }
    return Out;
}

int32 Undrained(const FGrid& Grid)
{
    TArray<uint8> Seen;
    Seen.SetNumZeroed(Grid.Num());
    TArray<int32> Queue;
    Queue.Reserve(Grid.Num());
    for (int32 Index = 0; Index < Grid.Num(); ++Index)
    {
        if (Grid.IsBorder(Index))
        {
            Seen[Index] = 1;
            Queue.Add(Index);
        }
    }
    // Up from the border to every neighbour at or above: water there runs back down the same way.
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 Cell = Queue[Head];
        ForEachNeighbour(Grid, Cell, [&](int32 Next, int32)
        {
            if (!Seen[Next] && Grid.Heights[Next] >= Grid.Heights[Cell])
            {
                Seen[Next] = 1;
                Queue.Add(Next);
            }
        });
    }
    return Grid.Num() - Queue.Num();
}
} // namespace McpTerrainErosion
