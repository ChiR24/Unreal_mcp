// Copyright (c) 2024 EngineWire Contributors
//
// The ground plan_road reads (McpRoadGround.h): vertical traces for what is under a column, and the lift that keeps the
// road laid between the route's points clear of it.

#include "Domains/Environment/Roads/McpRoadGround.h"

#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h"  // 5.0 declares FHitResult in EngineTypes.h
#endif
#if __has_include("Engine/OverlapResult.h")
#include "Engine/OverlapResult.h"  // EngineTypes.h held FOverlapResult before
#endif
#include "GameFramework/Volume.h"
#include "Landscape.h"
#include "LandscapeProxy.h"

namespace McpRoadGround
{
namespace
{
bool RoadIsGround(const AActor* Actor, const TSet<const AActor*>& Ground)
{
    // In a World Partition level the hits land on streaming proxies: a named landscape counts through them.
    const ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(Actor);
    return Ground.Contains(Actor) || (Proxy && (Ground.Num() == 0 || Ground.Contains(Proxy->GetLandscapeActor())));
}

// Solid as the placement checks judge support: volumes and anything that does not block WorldStatic are passed through.
bool RoadIsSolid(const AActor* Actor, const UPrimitiveComponent* Part)
{
    return Actor && Part && !Actor->IsA<AVolume>() && Part->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block;
}

// How far the ground between two points rises above the straight line joining their ground heights, traced every metre:
// the plan's cells are metres apart and cannot see a crest between two of them. Obstacles and gaps are not ground.
double RoadRise(UWorld* World, const FVector2D& A, double GroundA, const FVector2D& B, double GroundB, const TSet<const AActor*>& Ground)
{
    double Rise = 0.0;
    const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 100.0));
    for (int32 Step = 1; Step < Steps; ++Step)
    {
        const double Along = static_cast<double>(Step) / Steps;
        double Z = 0.0;
        AActor* Hit = nullptr;
        if (Probe(World, A + (B - A) * Along, Ground, Z, Hit) == ERoadGround::Ground)
        {
            Rise = FMath::Max(Rise, Z - FMath::Lerp(GroundA, GroundB, Along));
        }
    }
    return Rise;
}
} // namespace

// On ground, with HalfCell given, the box runs from 30 cm above the ground to a vehicle's height: one trace at the cell's
// middle let a 4 m wall between two cell centres through.
ERoadGround Probe(UWorld* World, const FVector2D& At, const TSet<const AActor*>& Ground, double& OutZ, AActor*& OutHit,
                  double HalfCell, const FQuat& Facing)
{
    FCollisionObjectQueryParams Solids;
    Solids.AddObjectTypesToQuery(ECC_WorldStatic);
    Solids.AddObjectTypesToQuery(ECC_WorldDynamic);
    Solids.AddObjectTypesToQuery(ECC_PhysicsBody);
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(McpPlanRoad), false);
    TArray<FHitResult> Hits;
    World->LineTraceMultiByObjectType(Hits, FVector(At, 500000.0), FVector(At, -500000.0), Solids, Params);
    for (const FHitResult& Hit : Hits)
    {
        if (!RoadIsSolid(Hit.GetActor(), Hit.GetComponent()))
        {
            continue;
        }
        OutZ = Hit.ImpactPoint.Z;
        OutHit = Hit.GetActor();
        if (!RoadIsGround(OutHit, Ground))
        {
            return ERoadGround::Obstacle;
        }
        TArray<FOverlapResult> Overlaps;
        if (HalfCell > 0.0)
        {
            World->OverlapMultiByObjectType(Overlaps, FVector(At, OutZ + 180.0), Facing, Solids, FCollisionShape::MakeBox(FVector(HalfCell, HalfCell, 150.0)), Params);
        }
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (RoadIsSolid(Overlap.GetActor(), Overlap.GetComponent()) && !RoadIsGround(Overlap.GetActor(), Ground))
            {
                OutHit = Overlap.GetActor();
                return ERoadGround::Obstacle;
            }
        }
        return ERoadGround::Ground;
    }
    return ERoadGround::Gap;
}

TArray<double> Lift(UWorld* World, const McpRoadPlan::FRoadField& Field, const TArray<FIntPoint>& Route,
                    TFunctionRef<FVector2D(const FIntPoint&)> WorldAt, const TSet<const AActor*>& Ground, double MaxGrade)
{
    const auto GroundAt = [&Field](const FIntPoint& Point) { return Field.Heights[Point.Y * Field.Width + Point.X]; };
    // 10 cm (the default road is an 8 cm slab with its middle on the route), and higher by the most the ground rises above
    // the straight road to either neighbour: a slab laid straight between points sank 20 cm into such a crest.
    TArray<double> Out;
    Out.Init(10.0, Route.Num());
    for (int32 Index = 0; Index + 1 < Route.Num(); ++Index)
    {
        const double Rise = 10.0 + RoadRise(World, WorldAt(Route[Index]), GroundAt(Route[Index]), WorldAt(Route[Index + 1]), GroundAt(Route[Index + 1]), Ground);
        Out[Index] = FMath::Max(Out[Index], Rise);
        Out[Index + 1] = FMath::Max(Out[Index + 1], Rise);
    }
    // A point lifted more than its neighbour steepens the road between them: the lower end goes up until no stretch is
    // steeper than the limit. Lifts only rise, so nothing sinks back in, and with equal lifts every stretch keeps the
    // grade its search allowed, so this ends.
    for (bool bChanged = true; bChanged;)
    {
        bChanged = false;
        for (int32 Index = 0; Index + 1 < Route.Num(); ++Index)
        {
            const double Run = FVector2D::Distance(WorldAt(Route[Index]), WorldAt(Route[Index + 1]));
            const double Climb = GroundAt(Route[Index + 1]) + Out[Index + 1] - GroundAt(Route[Index]) - Out[Index];
            const double Excess = FMath::Abs(Climb) - MaxGrade * Run;
            if (Excess > 0.01)
            {
                Out[Climb > 0.0 ? Index : Index + 1] += Excess;
                bChanged = true;
            }
        }
    }
    return Out;
}
} // namespace McpRoadGround
