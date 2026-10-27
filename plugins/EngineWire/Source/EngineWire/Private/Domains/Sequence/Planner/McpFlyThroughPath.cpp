// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFlyThrough.h"

#include "Algo/Reverse.h"
#include "HAL/PlatformTime.h"

namespace McpFlyThrough
{
namespace
{
constexpr double FlightMargin = 2000.0; // cm the search may stray beyond the box of the two points it joins
constexpr int32 FlightBits = 21;        // per axis in a cell's key: a million cells each way

// A grid of cells half the clearance apart, over the box round two points plus a margin. A cell is free when a
// sphere of the clearance plus half a diagonal step overlaps nothing, so the straight step between two free
// neighbours keeps the whole clearance (each of its points is within half a diagonal of one end).
struct FFlightGrid
{
    FVector Origin = FVector::ZeroVector;
    double Cell = 25.0;
    FIntVector Size = FIntVector::ZeroValue;

    bool Inside(const FIntVector& At) const
    {
        return At.X >= 0 && At.Y >= 0 && At.Z >= 0 && At.X < Size.X && At.Y < Size.Y && At.Z < Size.Z;
    }
    int64 Key(const FIntVector& At) const // only for cells Inside: unique there
    {
        return (static_cast<int64>(At.Z) << (2 * FlightBits)) | (static_cast<int64>(At.Y) << FlightBits) | At.X;
    }
    FVector Where(const FIntVector& At) const { return Origin + FVector(At.X, At.Y, At.Z) * Cell; }
    FIntVector Nearest(const FVector& Point) const
    {
        const FVector Local = (Point - Origin) / Cell;
        return FIntVector(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y), FMath::RoundToInt(Local.Z));
    }
};

FFlightGrid FlightGridFor(const FVector& From, const FVector& To, double Cell)
{
    FFlightGrid Grid;
    Grid.Cell = Cell;
    Grid.Origin = FVector::Min(From, To) - FVector(FlightMargin);
    const FVector Span = FVector::Max(From, To) + FVector(FlightMargin) - Grid.Origin;
    const int32 Most = (1 << FlightBits) - 1;
    Grid.Size = FIntVector(FMath::Min(FMath::CeilToInt(Span.X / Cell) + 1, Most), FMath::Min(FMath::CeilToInt(Span.Y / Cell) + 1, Most),
                           FMath::Min(FMath::CeilToInt(Span.Z / Cell) + 1, Most));
    return Grid;
}

using FFlightOpen = TPair<double, FIntVector>; // estimated whole cost, cell

// A* over the 26 neighbours of free cells, from the cell nearest From to the one nearest To (each joined to its point
// by a clear sweep); the points on the way, or none when the budget or the deadline runs out first.
TArray<FVector> FlightSearch(const FSpace& Space, const FVector& From, const FVector& To, FSearchBudget& Budget)
{
    const FFlightGrid Grid = FlightGridFor(From, To, Space.Radius * 0.5);
    const double Swollen = Space.Radius + Grid.Cell * 0.87; // half the diagonal of a cell
    const FIntVector Start = Grid.Nearest(From);
    const FIntVector Goal = Grid.Nearest(To);
    TMap<int64, bool> Freed;
    const auto IsFree = [&Space, &Grid, &Freed, Swollen](const FIntVector& At)
    {
        if (!Grid.Inside(At))
        {
            return false;
        }
        const int64 Key = Grid.Key(At);
        if (const bool* Known = Freed.Find(Key))
        {
            return *Known;
        }
        return Freed.Add(Key, Space.Free(Grid.Where(At), Swollen));
    };
    if (!IsFree(Start) || !IsFree(Goal) || !Space.Clear(From, Grid.Where(Start), Space.Radius) || !Space.Clear(Grid.Where(Goal), To, Space.Radius))
    {
        return {};
    }
    const auto Cheaper = [](const FFlightOpen& L, const FFlightOpen& R) { return L.Key < R.Key; };
    TMap<int64, double> Cost = {{Grid.Key(Start), 0.0}};
    TMap<int64, FIntVector> Came;
    TSet<int64> Closed;
    TArray<FFlightOpen> Open;
    Open.HeapPush(FFlightOpen(FVector::Dist(From, To), Start), Cheaper);
    while (Open.Num() > 0 && Budget.Spend())
    {
        FFlightOpen Top;
        Open.HeapPop(Top, Cheaper);
        const FIntVector At = Top.Value;
        bool bSeen = false;
        Closed.Add(Grid.Key(At), &bSeen);
        if (bSeen)
        {
            continue; // a stale entry: this cell was reached more cheaply already
        }
        if (At == Goal)
        {
            TArray<FVector> Path = {To};
            for (FIntVector Step = Goal; Step != Start; Step = Came[Grid.Key(Step)])
            {
                Path.Add(Grid.Where(Step));
            }
            Path.Add(Grid.Where(Start));
            Path.Add(From);
            Algo::Reverse(Path);
            return Path;
        }
        const double Spent = Cost[Grid.Key(At)];
        for (int32 Index = 0; Index < 27; ++Index)
        {
            const FIntVector Next = At + FIntVector(Index % 3 - 1, Index / 3 % 3 - 1, Index / 9 - 1);
            if (Next == At || !IsFree(Next) || Closed.Contains(Grid.Key(Next)))
            {
                continue;
            }
            const double Through = Spent + FVector::Dist(Grid.Where(At), Grid.Where(Next));
            const double* Known = Cost.Find(Grid.Key(Next));
            if (!Known || Through < *Known)
            {
                Cost.Add(Grid.Key(Next), Through);
                Came.Add(Grid.Key(Next), At);
                Open.HeapPush(FFlightOpen(Through + FVector::Dist(Grid.Where(Next), Grid.Where(Goal)), Next), Cheaper);
            }
        }
    }
    return {};
}

// Corners kept from Path: from each one, the farthest later one a clear sweep reaches (the next step always keeps the
// clearance: both its ends are free cells, or a point and the cell a sweep joined it to).
TArray<FVector> FlightCutShort(const FSpace& Space, const TArray<FVector>& Path, FSearchBudget& Budget)
{
    TArray<FVector> Kept = {Path[0]};
    for (int32 At = 0; At < Path.Num() - 1;)
    {
        int32 Reach = At + 1;
        for (int32 Far = Path.Num() - 1; Far > At + 1 && Budget.InTime(); --Far)
        {
            if (Space.Clear(Path[At], Path[Far], Space.Radius))
            {
                Reach = Far;
                break;
            }
        }
        Kept.Add(Path[Reach]);
        At = Reach;
    }
    return Kept;
}
} // namespace

bool FSearchBudget::Spend()
{
    return Cells-- > 0 && ((Cells & 255) != 0 || InTime());
}

bool FSearchBudget::InTime() const
{
    return FPlatformTime::Seconds() < Deadline;
}

FPlan PlanCorners(const FSpace& Space, const TArray<FVector>& Through, FSearchBudget& Budget)
{
    FPlan Plan;
    for (int32 Index = 0; Index < Through.Num(); ++Index)
    {
        if (!Space.Free(Through[Index], Space.Radius))
        {
            Plan.FailedPoint = Index;
            Plan.bInSolid = true;
            Plan.Error = FString::Printf(TEXT("through[%d] is nearer to geometry than the clearance, or inside it."), Index);
            return Plan;
        }
    }
    Plan.Points.Add(Through[0]);
    Plan.Given.Add(0);
    for (int32 Leg = 0; Leg + 1 < Through.Num(); ++Leg)
    {
        const FVector& From = Through[Leg];
        const FVector& To = Through[Leg + 1];
        const TArray<FVector> Path = Space.Clear(From, To, Space.Radius) ? TArray<FVector>{From, To} : FlightSearch(Space, From, To, Budget);
        if (Path.Num() < 2)
        {
            Plan.FailedPoint = Leg + 1;
            Plan.Error = Budget.Cells <= 0 || !Budget.InTime()
                ? FString::Printf(TEXT("The search between through[%d] and through[%d] ran out of cells or time: add points between them."), Leg, Leg + 1)
                : FString::Printf(TEXT("No way through free space joins through[%d] and through[%d] within %.0f m of them."), Leg, Leg + 1, FlightMargin / 100.0);
            return Plan;
        }
        const TArray<FVector> Corners = FlightCutShort(Space, Path, Budget);
        for (int32 Corner = 1; Corner < Corners.Num(); ++Corner)
        {
            Plan.Points.Add(Corners[Corner]);
        }
        Plan.Given.Add(Plan.Points.Num() - 1);
    }
    return Plan;
}
} // namespace McpFlyThrough
