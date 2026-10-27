// Copyright (c) 2024 EngineWire Contributors
//
// Reshaping a heightfield: valleys by stream power, gullies by droplets, slumps by talus (McpTerrainErosion.h).

#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"

namespace McpTerrainErosion
{
void StreamPower(FGrid& Grid, const FFlow& Flow, double WaveCm)
{
    double Largest = 0.0;
    for (int32 Index = 0; Index < Grid.Num(); ++Index)
    {
        if (Flow.Receiver[Index] != INDEX_NONE)
        {
            Largest = FMath::Max(Largest, Flow.Area[Index]);
        }
    }
    if (Largest <= 0.0)
    {
        return;
    }
    // Receivers first, so each cell moves toward its receiver's new height: with n = 1 the implicit step is exact for any
    // F. h_r + (h - h_r) / (1 + F), not (h + F h_r) / (1 + F): the second rounds a cell level with its receiver to just
    // below it, and every flat became a field of pits.
    for (const int32 Cell : Flow.Order)
    {
        const int32 Receiver = Flow.Receiver[Cell];
        if (Receiver == INDEX_NONE || Grid.Heights[Cell] <= Grid.Heights[Receiver])
        {
            continue;
        }
        const double F = WaveCm * FMath::Sqrt(Flow.Area[Cell] / Largest) / Flow.Distance[Cell];
        Grid.Heights[Cell] = Grid.Heights[Receiver] + (Grid.Heights[Cell] - Grid.Heights[Receiver]) / (1.0 + F);
    }
}

int32 Droplets(FGrid& Grid, int32 Count, int32 Seed, double Deadline)
{
    const int32 W = Grid.Width, H = Grid.Height;
    if (W < 5 || H < 5)
    {
        return 0;
    }
    // Heights in cells (h / cell size, so a slope of 1 is 45 degrees); Beyer's constants, a 2-cell erosion brush.
    constexpr double Inertia = 0.05, Capacity = 4.0, MinSlope = 0.01, ErodeRate = 0.3, DepositRate = 0.3, Evaporation = 0.02, Gravity = 4.0;
    constexpr int32 Life = 40, Radius = 2;
    const double Unit = FMath::Min(Grid.CellX, Grid.CellY);
    TArray<double>& Heights = Grid.Heights;
    struct FBrushCell
    {
        int32 X;
        int32 Y;
        double Weight;
    };
    TArray<FBrushCell> Brush;
    double WeightSum = 0.0;
    for (int32 Y = -Radius; Y <= Radius; ++Y)
    {
        for (int32 X = -Radius; X <= Radius; ++X)
        {
            const double Weight = 1.0 - FMath::Sqrt(static_cast<double>(X * X + Y * Y)) / Radius;
            if (Weight > 0.0)
            {
                Brush.Add({X, Y, Weight});
                WeightSum += Weight;
            }
        }
    }
    // Height (in cells) and slope at a point between cells, read bilinearly from its four corners.
    const auto Sample = [&](double X, double Y, double& OutSlopeX, double& OutSlopeY)
    {
        const int32 CX = FMath::FloorToInt(X), CY = FMath::FloorToInt(Y), I = CY * W + CX;
        const double U = X - CX, V = Y - CY;
        const double A = Heights[I] / Unit, B = Heights[I + 1] / Unit, C = Heights[I + W] / Unit, D = Heights[I + W + 1] / Unit;
        OutSlopeX = (B - A) * (1.0 - V) + (D - C) * V;
        OutSlopeY = (C - A) * (1.0 - U) + (D - B) * U;
        return A * (1.0 - U) * (1.0 - V) + B * U * (1.0 - V) + C * (1.0 - U) * V + D * U * V;
    };
    // Sediment laid on the four corners around a point; the border is held.
    const auto Drop = [&](double X, double Y, double Amount)
    {
        const int32 CX = FMath::FloorToInt(X), CY = FMath::FloorToInt(Y), I = CY * W + CX;
        const double U = X - CX, V = Y - CY;
        const int32 Corners[4] = {I, I + 1, I + W, I + W + 1};
        const double Weights[4] = {(1.0 - U) * (1.0 - V), U * (1.0 - V), (1.0 - U) * V, U * V};
        for (int32 K = 0; K < 4; ++K)
        {
            if (!Grid.IsBorder(Corners[K]))
            {
                Heights[Corners[K]] += Amount * Weights[K] * Unit;
            }
        }
    };
    FRandomStream Random(Seed);
    int32 Ran = 0;
    for (; Ran < Count; ++Ran)
    {
        if ((Ran & 1023) == 0 && FPlatformTime::Seconds() > Deadline)
        {
            break;
        }
        double X = 1.0 + Random.FRand() * (W - 3), Y = 1.0 + Random.FRand() * (H - 3);
        double DirX = 0.0, DirY = 0.0, Speed = 1.0, Water = 1.0, Sediment = 0.0;
        bool bLeft = false;
        for (int32 Step = 0; Step < Life; ++Step)
        {
            const int32 CX = FMath::FloorToInt(X), CY = FMath::FloorToInt(Y);
            double SlopeX = 0.0, SlopeY = 0.0;
            const double Here = Sample(X, Y, SlopeX, SlopeY);
            DirX = DirX * Inertia - SlopeX * (1.0 - Inertia);
            DirY = DirY * Inertia - SlopeY * (1.0 - Inertia);
            const double Length = FMath::Sqrt(DirX * DirX + DirY * DirY);
            if (Length <= 0.0)
            {
                break;
            }
            const double FromX = X, FromY = Y;
            X += DirX / Length;
            Y += DirY / Length;
            if (X < 1.0 || Y < 1.0 || X >= W - 2 || Y >= H - 2)
            {
                bLeft = true;
                break;
            }
            const double Rise = Sample(X, Y, SlopeX, SlopeY) - Here;
            const double Holds = FMath::Max(-Rise, MinSlope) * Speed * Water * Capacity;
            if (Sediment > Holds || Rise > 0.0)
            {
                // Uphill it fills the hollow behind it; over capacity it lets part go.
                const double Amount = Rise > 0.0 ? FMath::Min(Rise, Sediment) : (Sediment - Holds) * DepositRate;
                Sediment -= Amount;
                Drop(FromX, FromY, Amount);
            }
            else
            {
                // Never deeper than the drop ahead, so one droplet digs no hole.
                const double Amount = FMath::Min((Holds - Sediment) * ErodeRate, -Rise);
                for (const FBrushCell& Cell : Brush)
                {
                    const int32 BX = CX + Cell.X, BY = CY + Cell.Y;
                    if (BX >= 1 && BY >= 1 && BX <= W - 2 && BY <= H - 2)
                    {
                        Heights[BY * W + BX] -= Amount * Cell.Weight / WeightSum * Unit;
                    }
                }
                Sediment += Amount;
            }
            Speed = FMath::Sqrt(FMath::Max(0.0, Speed * Speed - Rise * Gravity));
            Water *= 1.0 - Evaporation;
        }
        // A droplet that runs dry or out of time lays down what it still carries: lost, every gully ended in a pit.
        if (!bLeft && Sediment > 0.0)
        {
            Drop(X, Y, Sediment);
        }
    }
    return Ran;
}

void Thermal(FGrid& Grid, double TalusDegrees, int32 Passes)
{
    const double Talus = FMath::Tan(FMath::DegreesToRadians(TalusDegrees));
    TArray<double> Delta;
    for (int32 Pass = 0; Pass < Passes; ++Pass)
    {
        // Every cell at once from the same heights, so the order cells are visited in does not matter.
        Delta.Init(0.0, Grid.Num());
        for (int32 Cell = 0; Cell < Grid.Num(); ++Cell)
        {
            if (Grid.IsBorder(Cell))
            {
                continue;
            }
            int32 Lower[8];
            double Excess[8];
            int32 Found = 0;
            double Most = 0.0, Total = 0.0;
            ForEachNeighbour(Grid, Cell, [&](int32 Next, int32 K)
            {
                const double Over = Grid.Heights[Cell] - Grid.Heights[Next] - Talus * Grid.Step(K);
                if (Over > 0.0)
                {
                    Lower[Found] = Next;
                    Excess[Found++] = Over;
                    Total += Over;
                    Most = FMath::Max(Most, Over);
                }
            });
            if (Found == 0)
            {
                continue;
            }
            // Half the largest excess slides off, shared by how far each lower neighbour is past the talus slope; what
            // reaches the held border leaves the grid.
            const double Slide = 0.5 * Most;
            Delta[Cell] -= Slide;
            for (int32 K = 0; K < Found; ++K)
            {
                if (!Grid.IsBorder(Lower[K]))
                {
                    Delta[Lower[K]] += Slide * Excess[K] / Total;
                }
            }
        }
        for (int32 Cell = 0; Cell < Grid.Num(); ++Cell)
        {
            Grid.Heights[Cell] += Delta[Cell];
        }
    }
}
} // namespace McpTerrainErosion
