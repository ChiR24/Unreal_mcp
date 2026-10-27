// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Foundation/Solvers/McpBoxSat.h"

class AActor;

// control_actor arrange_by_relations' model. Each actor is a box turned about Z: what it draws, measured in its own
// turned frame. The movers' places and turns are annealed (McpArrangeSolve.cpp) over the squared misses of the relations
// asked, how deep they overlap each other or the scene, and how far they wander from where they started (the poses the
// caller left them in, so the layout stays the caller's); then each contact is snapped exact (McpArrangeModel.cpp).
namespace McpArrange
{
struct FBody
{
    AActor* Actor = nullptr;
    FString Shape;                             // copies of one prop share it, and may trade places
    FVector Location = FVector::ZeroVector;    // the actor's origin
    double Yaw = 0.0;                          // radians
    FVector LocalCenter = FVector::ZeroVector; // the drawn box's centre in the actor's turned frame
    FVector Half = FVector::ZeroVector;        // half the drawn box's size in that frame
    FVector Seed = FVector::ZeroVector;        // where it started
    double SeedYaw = 0.0;
    bool bMover = false;
    bool bFreeYaw = true; // facing and against decide the turn
};

struct FGoal
{
    int32 Subject = INDEX_NONE; // body indices
    int32 Object = INDEX_NONE;
    FString Kind; // on, inside, nextTo, against, facing, near
    double Within = 100.0;
};

/** The body's footprint as it stands. */
McpBoxSat::FFootprint Footprint(const FBody& Body);

/** Its bottom and top. */
FVector2D Heights(const FBody& Body);

/** How far a goal misses at the bodies' poses, squared, in square metres (radians squared for a turn): 0 when it holds.
 *  With bMargins, what stands on a support also keeps off its edges where the top has room. */
double Miss(const TArray<FBody>& Bodies, const FGoal& Goal, bool bMargins = true);

/** The shift that makes an on, nextTo or against goal exact for its subject (inside its support's top, touching the
 *  other); zero for any other goal. */
FVector2D SnapBy(const TArray<FBody>& Bodies, const FGoal& Goal);

/** The solve's view of the bodies: what each takes part in and what moves or turns with what. */
struct FScene
{
    TArray<FBody>& Bodies;
    const TArray<FGoal>& Goals;
    TArray<int32> Movers;
    TArray<TArray<int32>> Carried;  // per body: the movers resting on it, at any depth
    TArray<TArray<int32>> GoalsOf;  // per body: the goals it takes part in
    TArray<TArray<int32>> TurnedBy; // per body: the subjects whose facing or against goal names it
    TArray<TArray<int32>> Swaps;    // per mover: its copies that rest on the same thing
    TSet<uint64> Joined;            // pairs an on or inside goal joins: they share space on purpose
    double Step = 100.0;            // cm of the first steps
};

inline uint64 PairKey(int32 A, int32 B)
{
    return (uint64(uint32(FMath::Min(A, B))) << 32) | uint32(FMath::Max(A, B));
}

/** Fills the scene's links, stands every mover on its support's top and turns it as its goals ask. */
void Link(FScene& Scene);

/** Turns a body as its facing goals, then its against goals, ask: a sofa against a wall faces the room. */
void Turn(FScene& Scene, int32 Body);

struct FSolve
{
    double Energy = 0.0;
    int32 Steps = 0;
    bool bDeadline = false;
};

/** Moves the movers until the goals hold as nearly as they can without overlaps: annealed for at most Steps or
 *  DeadlineSeconds, the best poses kept, then contacts snapped. Deterministic for a seed. */
FSolve Solve(TArray<FBody>& Bodies, const TArray<FGoal>& Goals, int32 Seed, int32 Steps, double DeadlineSeconds);
} // namespace McpArrange
