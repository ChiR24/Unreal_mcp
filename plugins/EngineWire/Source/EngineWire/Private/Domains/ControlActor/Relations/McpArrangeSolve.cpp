// Copyright (c) 2024 EngineWire Contributors
//
// arrange_by_relations' solve (McpArrange.h): Metropolis annealing over the movers' places and turns, the energy counted
// only for what a move touched, then the closing snap that makes each contact exact. A mover carries the movers resting
// on it; facing and against turn a mover whenever it or what it faces moves. Heights never change while annealing: an
// actor on another stands on its top from the start.

#include "Domains/ControlActor/Relations/McpArrange.h"

#include "Foundation/Solvers/McpAnneal.h"

namespace McpArrange
{
namespace
{
constexpr double DriftWeight = 0.02; // per m2 of wander from where a mover started
constexpr double TurnWeight = 0.01;  // per radian squared a mover turns from how it started: nothing turns for nothing
constexpr double SnapReach = 20.0;   // cm the closing snap may move a body
constexpr double QuarterTurn = 1.57079632679489662;

struct FPose
{
    FVector Location;
    double Yaw;
};

double OverlapOf(const FScene& Scene, int32 A, int32 B)
{
    const FVector2D HA = Heights(Scene.Bodies[A]);
    const FVector2D HB = Heights(Scene.Bodies[B]);
    if (FMath::Min(HA.Y, HB.Y) - FMath::Max(HA.X, HB.X) <= 1.0 || Scene.Joined.Contains(PairKey(A, B)))
    {
        return 0.0;
    }
    const McpBoxSat::FFootprint FA = Footprint(Scene.Bodies[A]);
    const McpBoxSat::FFootprint FB = Footprint(Scene.Bodies[B]);
    if (FVector2D::DistSquared(FA.Center, FB.Center) > FMath::Square(FA.Half.Size() + FB.Half.Size()))
    {
        return 0.0; // the bounding circles are apart: most pairs end here
    }
    return FMath::Square(McpBoxSat::Depth(FA, FB) / 100.0);
}

// The energy of every term a body of Subset takes part in, each counted once.
double Local(const FScene& Scene, const TArray<int32>& Subset)
{
    TSet<int32> In(Subset);
    TSet<int32> Counted;
    double Energy = 0.0;
    for (const int32 Body : Subset)
    {
        for (const int32 Goal : Scene.GoalsOf[Body])
        {
            bool bSeen = false;
            Counted.Add(Goal, &bSeen);
            Energy += bSeen ? 0.0 : Miss(Scene.Bodies, Scene.Goals[Goal]);
        }
        for (int32 Other = 0; Other < Scene.Bodies.Num(); ++Other)
        {
            Energy += Other == Body || (In.Contains(Other) && Other < Body) ? 0.0 : OverlapOf(Scene, Body, Other);
        }
        const FBody& Self = Scene.Bodies[Body];
        Energy += Self.bMover ? DriftWeight * FMath::Square(FVector2D(Self.Location - Self.Seed).Size() / 100.0) +
                                    TurnWeight * FMath::Square(FMath::FindDeltaAngleRadians(Self.SeedYaw, Self.Yaw))
                              : 0.0;
    }
    return Energy;
}

void Shift(FScene& Scene, int32 Body, const FVector2D& By)
{
    Scene.Bodies[Body].Location += FVector(By.X, By.Y, 0.0);
    for (const int32 Rider : Scene.Carried[Body])
    {
        Scene.Bodies[Rider].Location += FVector(By.X, By.Y, 0.0);
    }
}

struct FMove
{
    int32 Mover = INDEX_NONE;
    int32 Partner = INDEX_NONE; // a copy it trades places with
    double Roll = 0.0;          // what kind of change
};

// One random change: which mover, and a copy to trade places with now and then. Subset gets every body it touches.
FMove Choose(const FScene& Scene, FRandomStream& Random, TArray<int32>& Subset)
{
    FMove Move;
    Move.Mover = Scene.Movers[Random.RandRange(0, Scene.Movers.Num() - 1)];
    Move.Roll = Random.GetFraction();
    const TArray<int32>& Copies = Scene.Swaps[Move.Mover];
    Move.Partner = Move.Roll < 0.1 && Copies.Num() > 0 ? Copies[Random.RandRange(0, Copies.Num() - 1)] : INDEX_NONE;
    Subset.Reset();
    for (const int32 Body : {Move.Mover, Move.Partner})
    {
        if (Body != INDEX_NONE)
        {
            Subset.AddUnique(Body);
            Subset.Append(Scene.Carried[Body]);
        }
    }
    for (int32 Index = 0, Moved = Subset.Num(); Index < Moved; ++Index)
    {
        for (const int32 Turned : Scene.TurnedBy[Subset[Index]])
        {
            Subset.AddUnique(Turned);
        }
    }
    return Move;
}

// The change itself: a trade of places, a turn, or a step, smaller as the run goes on; then what faces anything moved turns.
void Apply(FScene& Scene, FRandomStream& Random, double Progress, const FMove& Move, const TArray<int32>& Subset)
{
    TArray<FBody>& Bodies = Scene.Bodies;
    if (Move.Partner != INDEX_NONE)
    {
        const FVector2D Between(Bodies[Move.Partner].Location - Bodies[Move.Mover].Location);
        Shift(Scene, Move.Mover, Between);
        Shift(Scene, Move.Partner, -Between);
    }
    else if (Move.Roll < 0.3 && Bodies[Move.Mover].bFreeYaw)
    {
        double& Yaw = Bodies[Move.Mover].Yaw;
        Yaw = Random.GetFraction() < 0.25 ? FMath::RoundToDouble(Yaw / QuarterTurn) * QuarterTurn
                                          : Yaw + Random.FRandRange(-2.0, 2.0) * QuarterTurn * (1.0 - 0.9 * Progress);
    }
    else
    {
        const double Size = FMath::Max(1.0, Scene.Step * (1.0 - 0.95 * Progress));
        Shift(Scene, Move.Mover, FVector2D(Random.FRandRange(-Size, Size), Random.FRandRange(-Size, Size)));
    }
    for (const int32 Body : Subset)
    {
        Turn(Scene, Body);
    }
}
} // namespace

FSolve Solve(TArray<FBody>& Bodies, const TArray<FGoal>& Goals, int32 Seed, int32 Steps, double DeadlineSeconds)
{
    FScene Scene{Bodies, Goals};
    Link(Scene);
    FSolve Out;
    if (Scene.Movers.Num() == 0)
    {
        return Out;
    }
    TArray<int32> Subset;
    TArray<FPose> Saved;
    TArray<FPose> Best;
    const auto Keep = [&Bodies](const TArray<int32>& Of, TArray<FPose>& Into)
    {
        Into.Reset();
        for (const int32 Body : Of)
        {
            Into.Add({Bodies[Body].Location, Bodies[Body].Yaw});
        }
    };
    const auto Restore = [&Bodies](const TArray<int32>& Of, const TArray<FPose>& From)
    {
        for (int32 Index = 0; Index < Of.Num(); ++Index)
        {
            Bodies[Of[Index]].Location = From[Index].Location;
            Bodies[Of[Index]].Yaw = From[Index].Yaw;
        }
    };
    McpAnneal::FSettings Settings;
    Settings.Steps = Steps;
    Settings.Seed = Seed;
    Settings.DeadlineSeconds = DeadlineSeconds;
    const McpAnneal::FOutcome Outcome = McpAnneal::Minimize(Local(Scene, Scene.Movers), Settings,
        [&](FRandomStream& Random, double Progress)
        {
            const FMove Move = Choose(Scene, Random, Subset);
            const double Before = Local(Scene, Subset);
            Keep(Subset, Saved);
            Apply(Scene, Random, Progress, Move, Subset);
            return Local(Scene, Subset) - Before;
        },
        [&]() { Restore(Subset, Saved); },
        [&]() { Keep(Scene.Movers, Best); });
    Restore(Scene.Movers, Best);
    // Annealing ends near a contact, rarely on it. A box turned to the other closes one axis at a time.
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        for (const FGoal& Goal : Goals)
        {
            const FVector2D By = Bodies[Goal.Subject].bMover ? SnapBy(Bodies, Goal) : FVector2D::ZeroVector;
            if (By.Size() > 0.01 && By.Size() <= SnapReach)
            {
                Shift(Scene, Goal.Subject, By);
            }
        }
    }
    Out.Energy = Local(Scene, Scene.Movers);
    Out.Steps = Outcome.StepsRun;
    Out.bDeadline = Outcome.bDeadline;
    return Out;
}
} // namespace McpArrange
