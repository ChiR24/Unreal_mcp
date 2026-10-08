// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// Which piece of each coplanar pair moves in one fix_coplanar pass. Moving the smaller piece of every pair moved each
// piece of a row of equal flush pieces (pier sections, floor tiles) the same way, so neighbours stayed in one plane and
// a pass freed only the end of the row: twelve sections were left as a staircase after four passes. The faces that
// touch in one plane are 2-coloured instead, outward from the larger piece of the largest overlap, and only one colour
// moves, so a row alternates and one pass frees it.

#include "Domains/ControlActor/Placement/McpAutomationBridge_CoplanarFaces.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h"

namespace McpCoplanar
{
namespace
{
// One face of one actor: the actor and its side, axis * 2 plus 1 for the + side.
using FMcpFaceKey = TPair<AActor*, int32>;

int32 McpCoplanarWorldAxis(const FVector& Normal)
{
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Normal[Axis]) > 0.99)
        {
            return Axis;
        }
    }
    return INDEX_NONE;
}

// Moves one face of Plan by Shift along the face's own direction. The largest overlap comes first and decides a face
// that two pairs share.
void McpCoplanarMoveFace(FMcpCoplanarPlan& Plan, const FVector& Normal, int32 Axis, double Shift)
{
    if (Axis == INDEX_NONE)
    {
        Plan.FreeShift += Normal * Shift;
    }
    else if (Normal[Axis] > 0.0 && !Plan.bPlus[Axis])
    {
        Plan.bPlus[Axis] = true;
        Plan.PlusShift[Axis] = Shift;
    }
    else if (Normal[Axis] < 0.0 && !Plan.bMinus[Axis])
    {
        Plan.bMinus[Axis] = true;
        Plan.MinusShift[Axis] = -Shift;
    }
}

// True for each face that moves. Every run of faces touching in one plane is coloured from the larger piece of its
// largest overlap, which stays; an odd ring keeps one pair alike, and the next pass meets it again.
TMap<FMcpFaceKey, bool> McpCoplanarColour(const TArray<FMcpCoplanarHit>& Hits, const TArray<int32>& Sides)
{
    TMap<FMcpFaceKey, TArray<int32>> Touching;
    for (int32 Index = 0; Index < Hits.Num(); ++Index)
    {
        if (Sides[Index] != INDEX_NONE)
        {
            Touching.FindOrAdd(FMcpFaceKey(Hits[Index].Actor, Sides[Index])).Add(Index);
            Touching.FindOrAdd(FMcpFaceKey(Hits[Index].OtherActor, Sides[Index])).Add(Index);
        }
    }
    TMap<FMcpFaceKey, bool> Moves;
    for (int32 Index = 0; Index < Hits.Num(); ++Index)
    {
        const FMcpFaceKey Start(Hits[Index].OtherActor, Sides[Index]);
        if (Sides[Index] == INDEX_NONE || Moves.Contains(Start))
        {
            continue;
        }
        Moves.Add(Start, false);
        TArray<FMcpFaceKey> Queue = {Start};
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FMcpFaceKey Face = Queue[Head];
            const bool bMoves = Moves.FindRef(Face);
            for (const int32 Pair : Touching.FindRef(Face))
            {
                const FMcpFaceKey Next(Hits[Pair].Actor == Face.Key ? Hits[Pair].OtherActor : Hits[Pair].Actor, Face.Value);
                if (!Moves.Contains(Next))
                {
                    Moves.Add(Next, !bMoves);
                    Queue.Add(Next);
                }
            }
        }
    }
    return Moves;
}
} // namespace

TMap<AActor*, FMcpCoplanarPlan> PlanCoplanarPass(const TArray<FMcpCoplanarHit>& Hits, double Distance,
                                                 TArray<FMcpCoplanarHit>& Inside)
{
    TArray<int32> Sides;
    for (const FMcpCoplanarHit& Hit : Hits)
    {
        const int32 Axis = McpCoplanarWorldAxis(Hit.Normal);
        const bool bColoured = Axis != INDEX_NONE && Hit.Actor != Hit.OtherActor;
        Sides.Add(bColoured ? Axis * 2 + (Hit.Normal[Axis] > 0.0 ? 1 : 0) : INDEX_NONE);
    }
    const TMap<FMcpFaceKey, bool> Moves = McpCoplanarColour(Hits, Sides);
    TMap<AActor*, FMcpCoplanarPlan> Plans;
    for (int32 Index = 0; Index < Hits.Num(); ++Index)
    {
        const FMcpCoplanarHit& Hit = Hits[Index];
        // An actor tagged mcp.placement.ok overlaps on purpose, but no two faces flicker on purpose: it is fixed too.
        if (Hit.Actor == Hit.OtherActor)
        {
            Inside.Add(Hit);
            continue;
        }
        // The smaller piece comes forward when it lies wholly inside the other face and goes back when it is only
        // partly inside; when the colouring keeps it, the other piece moves the opposite way instead.
        const bool bApplied = Hit.FaceArea > 0.0 && Hit.OverlapU * Hit.OverlapV >= 0.95 * Hit.FaceArea;
        double Shift = bApplied ? Distance : -Distance; // along the face's own direction
        AActor* Mover = Hit.Actor;
        AActor* Still = Hit.OtherActor;
        const int32 Side = Sides[Index];
        if (Side != INDEX_NONE && !Moves.FindRef(FMcpFaceKey(Mover, Side)) && Moves.FindRef(FMcpFaceKey(Still, Side)))
        {
            Swap(Mover, Still);
            Shift = -Shift;
        }
        FMcpCoplanarPlan& Plan = Plans.FindOrAdd(Mover);
        McpCoplanarMoveFace(Plan, Hit.Normal, Side == INDEX_NONE ? INDEX_NONE : Side / 2, Shift);
        Plan.Pairs.Add(MakeShared<FJsonValueString>(
            FString::Printf(TEXT("%s face with '%s' %s"), *DescribeDirection(Hit.Normal), *McpActorRef(Still),
                            Shift > 0.0 ? TEXT("brought forward") : TEXT("pulled back"))));
    }
    return Plans;
}
} // namespace McpCoplanar
