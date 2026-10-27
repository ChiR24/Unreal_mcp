// Copyright (c) 2024 EngineWire Contributors
//
// The rivers of a drained heightfield (McpTerrainErosion.h).

#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

#include "Algo/Reverse.h"

namespace McpTerrainErosion
{
namespace
{
// Douglas-Peucker on the plan, without recursion (a river can run thousands of cells): the points that stand more than
// Tolerance cells off the line between their kept neighbours.
TArray<int32> RiverKeep(const TArray<FVector>& Points, double Tolerance)
{
    TArray<bool> Keep;
    Keep.Init(false, Points.Num());
    Keep[0] = Keep.Last() = true;
    TArray<TPair<int32, int32>> Spans = {{0, Points.Num() - 1}};
    while (Spans.Num() > 0)
    {
        const TPair<int32, int32> Span = Spans.Pop();
        const FVector2D A(Points[Span.Key]), B(Points[Span.Value]);
        const double Length = FMath::Max(FVector2D::Distance(A, B), 1e-9);
        double Worst = 0.0;
        int32 At = INDEX_NONE;
        for (int32 Index = Span.Key + 1; Index < Span.Value; ++Index)
        {
            const double Off = FMath::Abs(FVector2D::CrossProduct(B - A, FVector2D(Points[Index]) - A)) / Length;
            if (Off > Worst)
            {
                Worst = Off;
                At = Index;
            }
        }
        if (At != INDEX_NONE && Worst > Tolerance)
        {
            Keep[At] = true;
            Spans.Add({Span.Key, At});
            Spans.Add({At, Span.Value});
        }
    }
    TArray<int32> Kept;
    for (int32 Index = 0; Index < Keep.Num(); ++Index)
    {
        if (Keep[Index])
        {
            Kept.Add(Index);
        }
    }
    return Kept;
}
} // namespace

TArray<FRiver> Rivers(const FGrid& Grid, const FFlow& Flow, double AreaCm2, int32 Max)
{
    struct FRun
    {
        double Catchment = 0.0;
        TArray<int32> Cells;  // mouth first
    };
    // A river starts at each cell that drains into a held cell, or into a larger river (a junction found on the way).
    TArray<TPair<int32, int32>> Starts;
    for (int32 Cell = 0; Cell < Grid.Num(); ++Cell)
    {
        const int32 Receiver = Flow.Receiver[Cell];
        if (Receiver != INDEX_NONE && Flow.Receiver[Receiver] == INDEX_NONE && Flow.Area[Cell] >= AreaCm2)
        {
            Starts.Add({Receiver, Cell});
        }
    }
    TArray<FRun> Runs;
    for (int32 Next = 0; Next < Starts.Num(); ++Next)
    {
        FRun Run;
        Run.Catchment = Flow.Area[Starts[Next].Value];
        Run.Cells = {Starts[Next].Key, Starts[Next].Value};
        // Up the largest donor that is a river; every other river donor starts a tributary ending here.
        for (int32 Cell = Starts[Next].Value;;)
        {
            int32 Best = INDEX_NONE;
            for (int32 At = Flow.DonorStart[Cell]; At < Flow.DonorStart[Cell + 1]; ++At)
            {
                const int32 Donor = Flow.Donors[At];
                if (Flow.Area[Donor] >= AreaCm2 && (Best == INDEX_NONE || Flow.Area[Donor] > Flow.Area[Best]))
                {
                    Best = Donor;
                }
            }
            if (Best == INDEX_NONE)
            {
                break;
            }
            for (int32 At = Flow.DonorStart[Cell]; At < Flow.DonorStart[Cell + 1]; ++At)
            {
                const int32 Donor = Flow.Donors[At];
                if (Donor != Best && Flow.Area[Donor] >= AreaCm2)
                {
                    Starts.Add({Cell, Donor});
                }
            }
            Run.Cells.Add(Best);
            Cell = Best;
        }
        Runs.Add(MoveTemp(Run));
    }
    Runs.StableSort([](const FRun& A, const FRun& B) { return A.Catchment > B.Catchment; });

    TArray<FRiver> Out;
    for (FRun& Run : Runs)
    {
        if (Out.Num() >= Max)
        {
            break;
        }
        Algo::Reverse(Run.Cells);
        FRiver& River = Out.AddDefaulted_GetRef();
        River.CatchmentM2 = Run.Catchment / 10000.0;
        TArray<FVector> Points;
        for (int32 Index = 0; Index < Run.Cells.Num(); ++Index)
        {
            const int32 Cell = Run.Cells[Index];
            Points.Add(FVector(Cell % Grid.Width, Cell / Grid.Width, Grid.Heights[Cell]));
            if (Index > 0)
            {
                River.LengthCm += FVector2D::Distance(FVector2D(Points.Last()) * FVector2D(Grid.CellX, Grid.CellY),
                                                      FVector2D(Points[Index - 1]) * FVector2D(Grid.CellX, Grid.CellY));
            }
        }
        // At most 64 points: the tolerance doubles until the line fits.
        TArray<int32> Kept;
        double Tolerance = 1.0;
        do
        {
            Kept = RiverKeep(Points, Tolerance);
            Tolerance *= 2.0;
        } while (Kept.Num() > 64);
        for (const int32 Index : Kept)
        {
            River.Points.Add(Points[Index]);
        }
    }
    return Out;
}
} // namespace McpTerrainErosion
