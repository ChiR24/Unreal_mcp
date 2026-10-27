// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFlyThrough.h"

namespace McpFlyThrough
{
namespace
{
constexpr int32 FlightRounds = 8;
constexpr double FlightSlack = 0.8; // a curve may pass this share of the clearance from geometry (the corners' lines keep all of it)

double FlightLength(const TArray<FVector>& Points)
{
    double Length = 0.0;
    for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
    {
        Length += FVector::Dist(Points[Index], Points[Index + 1]);
    }
    return Length;
}
} // namespace

bool Fly(const FSpace& Space, FPlan& Plan, double Seconds, const FSearchBudget& Budget, TArray<McpMinSnap::FLeg>& OutLegs,
         FString& OutError)
{
    for (int32 Round = 0; Round < FlightRounds && Budget.InTime(); ++Round)
    {
        const double Length = FMath::Max(FlightLength(Plan.Points), 1.0);
        TArray<double> Times;
        for (int32 Leg = 0; Leg + 1 < Plan.Points.Num(); ++Leg)
        {
            Times.Add(FMath::Max(Seconds * FVector::Dist(Plan.Points[Leg], Plan.Points[Leg + 1]) / Length, 0.05));
        }
        if (!McpMinSnap::Solve(Plan.Points, Times, OutLegs))
        {
            OutError = TEXT("The flight could not be smoothed (two points coincide).");
            return false;
        }
        // Each leg walked in steps of about 10 cm; one that strays gets the middle of its straight line as a corner.
        TArray<int32> Strays;
        for (int32 Leg = 0; Leg < OutLegs.Num(); ++Leg)
        {
            const int32 Steps = FMath::Clamp(FMath::CeilToInt(FVector::Dist(Plan.Points[Leg], Plan.Points[Leg + 1]) / 10.0), 8, 400);
            FVector Last = Plan.Points[Leg];
            for (int32 Step = 1; Step <= Steps; ++Step)
            {
                const FVector Next = McpMinSnap::Evaluate(OutLegs[Leg], OutLegs[Leg].Seconds * Step / Steps);
                if (!Space.Clear(Last, Next, Space.Radius * FlightSlack))
                {
                    Strays.Add(Leg);
                    break;
                }
                Last = Next;
            }
        }
        if (Strays.Num() == 0)
        {
            return true;
        }
        for (int32 Index = Strays.Num() - 1; Index >= 0; --Index)
        {
            const int32 Leg = Strays[Index];
            Plan.Points.Insert((Plan.Points[Leg] + Plan.Points[Leg + 1]) * 0.5, Leg + 1);
            for (int32& Given : Plan.Given)
            {
                Given += Given > Leg ? 1 : 0;
            }
        }
    }
    OutError = Budget.InTime() ? TEXT("The smoothed flight still strays nearer to geometry than the clearance: add points between, or lower clearanceCm.")
                               : TEXT("The flight could not be smoothed in time: add points between, or lower clearanceCm.");
    return false;
}

FVector Position(const TArray<McpMinSnap::FLeg>& Legs, double T)
{
    for (const McpMinSnap::FLeg& Leg : Legs)
    {
        if (T <= Leg.Seconds || &Leg == &Legs.Last())
        {
            return McpMinSnap::Evaluate(Leg, FMath::Clamp(T, 0.0, Leg.Seconds));
        }
        T -= Leg.Seconds;
    }
    return FVector::ZeroVector;
}
} // namespace McpFlyThrough
