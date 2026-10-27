// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Domains/Environment/Roads/McpRoadPlan.h"
#include "Templates/Function.h"

class AActor;
class UWorld;

// What plan_road reads from the level (McpRoadGround.cpp): the ground under a column, and how high above it the route's
// points go so that the road laid on them clears it.
namespace McpRoadGround
{
enum class ERoadGround : uint8
{
    Gap,
    Ground,
    Obstacle,
};

/** The first solid thing under a column: ground (a landscape, or one of Ground), an obstacle, or nothing (a gap). With
    HalfCell, a box over the whole cell (turned by Facing) finds anything else standing on the ground there. */
ERoadGround Probe(UWorld* World, const FVector2D& At, const TSet<const AActor*>& Ground, double& OutZ, AActor*& OutHit,
                  double HalfCell = 0.0, const FQuat& Facing = FQuat::Identity);

/** How far above its ground each route point goes: 10 cm, more where the ground between it and a neighbour rises above
    the straight road joining them, then the lower end of any stretch steeper than MaxGrade raised until none is. */
TArray<double> Lift(UWorld* World, const McpRoadPlan::FRoadField& Field, const TArray<FIntPoint>& Route,
                    TFunctionRef<FVector2D(const FIntPoint&)> WorldAt, const TSet<const AActor*>& Ground, double MaxGrade);
} // namespace McpRoadGround
