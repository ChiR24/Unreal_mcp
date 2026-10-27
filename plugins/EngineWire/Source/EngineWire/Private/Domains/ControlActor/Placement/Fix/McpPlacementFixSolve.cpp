// Copyright (c) 2024 EngineWire Contributors
//
// The separation half of fix_placement: projected Gauss-Seidel over the intersecting pairs. Each sweep measures every
// pair at the offsets its bodies have reached (McpMeasurePush: the shortest way out, on their collision), splits the
// push by inverse mass and goes on to the next pair with the new offsets, so a pile of three settles in a few sweeps
// instead of one pair undoing the last.

#include "Domains/ControlActor/Placement/Fix/McpPlacementFix.h"

#include "Domains/ControlActor/Placement/McpPlacementShapes.h"

namespace McpPlacementFix
{
namespace
{
// Depths below this are touching. A push leaves the pair this far apart, so a chain stays settled while its neighbours
// move: stopping at the gap itself left a row of three cubes 0.45 into each other.
constexpr double TouchDepth = 0.1;
constexpr double PushGap = 0.5;
constexpr int32 MaxSweeps = 8;
} // namespace

FSolveResult Separate(TArray<FBody>& Bodies, const TArray<TPair<int32, int32>>& Pairs, int32 MaxCalls)
{
    FSolveResult Result;
    TArray<bool> Measured;
    TArray<bool> Pushed;
    TArray<bool> ApartWhereTheyStand;
    Measured.Init(false, Pairs.Num());
    Pushed.Init(false, Pairs.Num());
    ApartWhereTheyStand.Init(false, Pairs.Num());
    while (Result.Sweeps < MaxSweeps && !Result.bLimitReached)
    {
        ++Result.Sweeps;
        double Deepest = 0.0;
        for (int32 Index = 0; Index < Pairs.Num(); ++Index)
        {
            if (Result.Calls >= MaxCalls)
            {
                Result.bLimitReached = true;
                break;
            }
            FBody& A = Bodies[Pairs[Index].Key];
            FBody& B = Bodies[Pairs[Index].Value];
            const double Total = A.InverseMass + B.InverseMass;
            FVector Push = FVector::ZeroVector;
            if (Total <= 0.0 || !McpPlacement::McpMeasurePush(A.Actor, B.Actor, A.Offset - B.Offset, Push, Result.Calls))
            {
                continue;
            }
            const double Depth = Push.Size();
            // Apart only where the two stand (no planned move between them yet): the audit judged them by their boxes.
            // A pair a lift or another push already parted is fixed, not apart.
            if (!Measured[Index] && Depth < TouchDepth && (A.Offset - B.Offset).IsNearlyZero())
            {
                ApartWhereTheyStand[Index] = true;
            }
            Measured[Index] = true;
            if (Depth < TouchDepth)
            {
                continue;
            }
            Pushed[Index] = true;
            Deepest = FMath::Max(Deepest, Depth);
            const FVector Step = Push * ((Depth + PushGap) / Depth);
            A.Offset += Step * (A.InverseMass / Total);
            B.Offset -= Step * (B.InverseMass / Total);
        }
        if (Deepest < TouchDepth)
        {
            break;
        }
    }
    for (int32 Index = 0; Index < Pairs.Num(); ++Index)
    {
        if (!Measured[Index])
        {
            Result.Unmeasured.Add(Pairs[Index]);
        }
        else if (!Pushed[Index] && ApartWhereTheyStand[Index])
        {
            Result.Apart.Add(Pairs[Index]);
        }
    }
    return Result;
}
} // namespace McpPlacementFix
