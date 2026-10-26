// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/Optimize/McpMinSnap.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// manage_sequence fly_through: a camera flown through given points and keyed into a Level Sequence, never nearer to
// geometry than a clearance. Each pair of points is joined by a straight line when a sphere of the clearance sweeps
// along it untouched, else by A* over a grid of free cells (a cell is free when that sphere overlaps nothing) cut short
// wherever a sweep allows; the corners are smoothed into one minimum-snap trajectory (Foundation/Optimize/McpMinSnap)
// timed by length, and a leg whose curve strays toward geometry gets the middle of its straight line as one more corner.
namespace McpFlyThrough
{
/** What the world answers for the planner; injected so the planner can be tested without a level. */
struct FSpace
{
    double Radius = 50.0;                                                          // the clearance kept
    TFunction<bool(const FVector& Point, double Radius)> Free;                     // a sphere there overlaps nothing
    TFunction<bool(const FVector& From, const FVector& To, double Radius)> Clear; // and sweeps along untouched
};

struct FPlan
{
    TArray<FVector> Points;          // the corners the flight passes, the given points among them
    TArray<int32> Given;             // where each given point sits in Points
    FString Error;                   // empty when planned
    int32 FailedPoint = INDEX_NONE;  // the given point that could not be reached (or stands in geometry)
    bool bInSolid = false;
};

/** What the search may spend: cells A* expands (shared by every leg) and a deadline (FPlatformTime::Seconds()). */
struct FSearchBudget
{
    int32 Cells = 100000;
    double Deadline = TNumericLimits<double>::Max();
    bool Spend();
    bool InTime() const;
};

/** Corners joining Through in free space: A* over cells half the clearance apart, cut short by sweeps. */
FPlan PlanCorners(const FSpace& Space, const TArray<FVector>& Through, FSearchBudget& Budget);

/** The minimum-snap flight through Plan's corners over Seconds, legs timed by length; a leg that strays nearer to
 *  geometry than 0.8 x the clearance gets the middle of its straight line as a corner (8 rounds, within the deadline). */
bool Fly(const FSpace& Space, FPlan& Plan, double Seconds, const FSearchBudget& Budget, TArray<McpMinSnap::FLeg>& OutLegs,
         FString& OutError);

/** Where the flight is T seconds in. */
FVector Position(const TArray<McpMinSnap::FLeg>& Legs, double T);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpFlyThrough
