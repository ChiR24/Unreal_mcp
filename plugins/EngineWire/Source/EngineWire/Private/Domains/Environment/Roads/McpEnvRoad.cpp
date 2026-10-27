// Copyright (c) 2024 EngineWire Contributors
//
// build_environment plan_road: a road between two points over the ground, under a grade limit, round obstacles, with no
// turn tighter than a radius. The ground under a grid of cells is read by vertical traces (a landscape, or the actors
// named as ground; anything else solid is an obstacle, nothing at all a gap), the route is found and straightened by
// McpRoadPlan, and the reply hands it to create_road_spline and configure_landscape_splines as ready calls.

#include "Domains/Environment/Roads/McpRoadPlan.h"

#include "Dom/JsonObject.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineWireSubsystem.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h"  // 5.0 declares FHitResult in EngineTypes.h
#endif
#if __has_include("Engine/OverlapResult.h")
#include "Engine/OverlapResult.h"  // EngineTypes.h held FOverlapResult before
#endif
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "GameFramework/Volume.h"
#include "HAL/PlatformTime.h"
#include "Landscape.h"
#include "LandscapeProxy.h"

namespace McpRoadPlanning
{
namespace
{
enum class ERoadGround : uint8
{
    Gap,
    Ground,
    Obstacle,
};

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

// The first solid thing under a column. On ground, with HalfCell given, a box over the whole cell (turned as the grid is)
// from 30 cm above the ground to a vehicle's height finds anything else standing on it: one trace at the cell's middle
// let a 4 m wall between two cell centres through.
ERoadGround RoadProbe(UWorld* World, const FVector2D& At, const TSet<const AActor*>& Ground, double& OutZ, AActor*& OutHit,
                      double HalfCell = 0.0, const FQuat& Facing = FQuat::Identity)
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

bool RoadReadPoint(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, FVector2D& Out)
{
    const TSharedPtr<FJsonObject>* Point = nullptr;
    return Payload->TryGetObjectField(Field, Point) && Point && (*Point)->TryGetNumberField(TEXT("x"), Out.X) && (*Point)->TryGetNumberField(TEXT("y"), Out.Y);
}

FString RoadWhyBlocked(const TCHAR* Which, ERoadGround Kind, const AActor* Hit)
{
    return Kind == ERoadGround::Obstacle
               ? FString::Printf(TEXT("%s is on '%s', an obstacle: name it in groundActors if a road may run on it"), Which, *Hit->GetActorLabel())
               : FString::Printf(TEXT("%s has no collision under it (a create_primitive mesh has none until configure_mesh_collision)"), Which);
}

TSharedPtr<FJsonObject> RoadCall(const TCHAR* Action, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
    Call->SetStringField(TEXT("operation"), TEXT("execute"));
    Call->SetStringField(TEXT("tool"), TEXT("build_environment"));
    Call->SetStringField(TEXT("action"), Action);
    Call->SetObjectField(TEXT("params"), Params);
    return Call;
}

double RoadRound(double Value, double Per)
{
    return FMath::RoundToDouble(Value * Per) / Per;
}
} // namespace

bool HandlePlanRoad(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const double Start = FPlatformTime::Seconds();
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    FVector2D From, To;
    if (!World || !RoadReadPoint(Payload, TEXT("from"), From) || !RoadReadPoint(Payload, TEXT("to"), To) || FVector2D::Distance(From, To) < 100.0)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("from and to are {x, y} in world cm, at least 1 m apart."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double MaxGrade = FMath::Clamp(GetJsonNumberField(Payload, TEXT("maxGradePercent"), 10.0), 1.0, 60.0) / 100.0;
    const double Width = FMath::Clamp(GetJsonNumberField(Payload, TEXT("width"), 600.0), 100.0, 5000.0);
    const double Radius = FMath::Clamp(GetJsonNumberField(Payload, TEXT("turnRadius"), 1500.0), 200.0, 20000.0);
    TSet<const AActor*> Ground;
    for (const FString& Name : McpGetStringListField(Payload, TEXT("groundActors"), TEXT("")))
    {
        AActor* Actor = FindActorByNameInWorldForMcp(World, Name, true);
        if (!Actor)
        {
            Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor '%s' to drive on."), *Name), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Ground.Add(Actor);
    }

    double FromZ = 0.0, ToZ = 0.0;
    AActor* FromHit = nullptr;
    AActor* ToHit = nullptr;
    // With no groundActors, a road from something other than a landscape runs on that something (a floor mesh).
    if (Ground.Num() == 0 && RoadProbe(World, From, Ground, FromZ, FromHit) == ERoadGround::Obstacle)
    {
        Ground.Add(FromHit);
    }
    // The ends are judged by their own points, not their cells: a road may end at a door, its cell touching the wall.
    const ERoadGround FromKind = RoadProbe(World, From, Ground, FromZ, FromHit);
    const ERoadGround ToKind = RoadProbe(World, To, Ground, ToZ, ToHit);
    if (FromKind != ERoadGround::Ground || ToKind != ERoadGround::Ground)
    {
        Bridge.SendAutomationError(Socket, RequestId, FromKind != ERoadGround::Ground ? RoadWhyBlocked(TEXT("from"), FromKind, FromHit) : RoadWhyBlocked(TEXT("to"), ToKind, ToHit),
                                   TEXT("POINT_BLOCKED"));
        return true;
    }
    // The grid over both ends and a margin: half their distance, or half the road a climb between them needs (at least
    // climb / grade long), since a steep climb winds and half the distance left a 40 m hill no room to. A cell is at least
    // 0.48 x the turn radius: one heading notch between two one-cell edges turns at 2.12 cells, so finer cells could not
    // take it, and coarser ones lose the tightest turns (a 3% larger cell could no longer climb a hill at the limit).
    const double Distance = FVector2D::Distance(From, To);
    double Margin = FMath::Max3(0.5 * Distance, 5000.0, 0.5 * FMath::Abs(ToZ - FromZ) / MaxGrade);
    const double Wanted = Distance / FMath::Max3(0.48 * Radius, (Distance + 2.0 * Margin) / 400.0, 100.0);
    // Both ends sit on grid points, to i cells along and j across from from (ends snapped to cell centres began and ended
    // the road up to half a cell diagonal off): the grid is turned to fit, the cell the smallest at least the one wanted.
    FIntPoint Reach(1, 0);
    for (int32 I = 1; I <= FMath::FloorToInt(Wanted); ++I)
    {
        const int32 J = FMath::Min(I, FMath::FloorToInt(FMath::Sqrt(FMath::Max(0.0, Wanted * Wanted - I * I))));
        Reach = I * I + J * J >= Reach.X * Reach.X + Reach.Y * Reach.Y ? FIntPoint(I, J) : Reach;  // a tie: straighter
    }
    McpRoadPlan::FRoadField Field;
    Field.Cell = Distance / FMath::Sqrt(static_cast<double>(Reach.X * Reach.X + Reach.Y * Reach.Y));
    // Ends nearer than one cell are one smaller cell apart, the margin cut to keep at most 400 cells a side.
    Margin = FMath::Min(Margin, (400.0 * Field.Cell - Distance) / 2.0);
    const int32 Rim = FMath::CeilToInt(Margin / Field.Cell);
    Field.Width = 2 * Rim + Reach.X + 1;
    Field.Height = 2 * Rim + Reach.Y + 1;
    Field.Heights.Init(0.0, Field.Width * Field.Height);
    Field.Open.Init(0, Field.Width * Field.Height);
    const FIntPoint FromCell(Rim, Rim), ToCell(Rim + Reach.X, Rim + Reach.Y);
    const double Yaw = FMath::Atan2(To.Y - From.Y, To.X - From.X) - FMath::Atan2(static_cast<double>(Reach.Y), static_cast<double>(Reach.X));
    const FVector2D Along(FMath::Cos(Yaw), FMath::Sin(Yaw)), Across(-Along.Y, Along.X);
    const auto WorldAt = [&](const FIntPoint& Cell) { return From + Along * ((Cell.X - Rim) * Field.Cell) + Across * ((Cell.Y - Rim) * Field.Cell); };
    const FQuat Facing(FVector::UpVector, Yaw);
    ALandscapeProxy* Landscape = nullptr;
    for (int32 Y = 0; Y < Field.Height; ++Y)
    {
        for (int32 X = 0; X < Field.Width; ++X)
        {
            AActor* Hit = nullptr;
            const ERoadGround Kind = RoadProbe(World, WorldAt(FIntPoint(X, Y)), Ground, Field.Heights[Y * Field.Width + X], Hit, Field.Cell / 2.0, Facing);
            const bool bEnd = FIntPoint(X, Y) == FromCell || FIntPoint(X, Y) == ToCell;
            Field.Open[Y * Field.Width + X] = Kind == ERoadGround::Ground || bEnd ? 1 : 0;
            Landscape = Landscape ? Landscape : Kind == ERoadGround::Ground ? Cast<ALandscapeProxy>(Hit) : nullptr;
        }
    }
    // Obstacles and gaps keep half the width away, except round the two ends, so a road can end at a door.
    McpRoadPlan::Grow(Field, FMath::CeilToInt(Width / 2.0 / Field.Cell), {FromCell, ToCell});
    int32 Searched = 0;
    bool bStopped = false;
    // Both searches inside 22 s of the call, well under the client's 30.
    const TArray<FIntPoint> Lattice = McpRoadPlan::Search(Field, FromCell, ToCell, MaxGrade, Radius, Start + 16.0, Searched, bStopped);
    if (Lattice.Num() == 0)
    {
        // Why: the slope (a route steeper than the limit exists) or the ground (none at all).
        int32 Again = 0;
        bool bAgainStopped = false;
        const TArray<FIntPoint> Steep = bStopped ? TArray<FIntPoint>() : McpRoadPlan::Search(Field, FromCell, ToCell, 10.0, Radius, Start + 22.0, Again, bAgainStopped);
        const FString Why = bStopped ? FString(TEXT("the search stopped at its time or memory limit"))
                          : Steep.Num() > 0 ? FString::Printf(TEXT("only routes steeper than the limit reach it (one climbs %.0f%%): raise maxGradePercent or lower turnRadius"),
                                                              McpRoadPlan::Measure(Field, McpRoadPlan::Smooth(Field, Steep, 10.0, Radius)).Grade * 100.0)
                          : bAgainStopped ? FString(TEXT("none under the grade limit, and whether a steeper one exists was not settled in time"))
                                          : FString::Printf(TEXT("obstacles or gaps in the ground cut the ends apart for a road %.0f cm wide"), Width);
        Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No road: %s (%d states searched in %.1f s)."), *Why, Searched + Again,
                                                                      FPlatformTime::Seconds() - Start), TEXT("NO_ROUTE"));
        return true;
    }
    const TArray<FIntPoint> Route = McpRoadPlan::Smooth(Field, Lattice, MaxGrade, Radius);
    const McpRoadPlan::FMeasure Laid = McpRoadPlan::Measure(Field, Route);

    TArray<TSharedPtr<FJsonValue>> Points, SplinePoints;
    for (const FIntPoint& Point : Route)
    {
        const FVector2D Flat = WorldAt(Point);
        const double Z = Field.Heights[Point.Y * Field.Width + Point.X] + 10.0;
        const TSharedPtr<FJsonObject> At = McpHandlerUtils::VectorToJson(FVector(RoadRound(Flat.X, 1.0), RoadRound(Flat.Y, 1.0), RoadRound(Z, 1.0)));
        Points.Add(MakeShared<FJsonValueObject>(At));
        const TSharedPtr<FJsonObject> Spline = MakeShared<FJsonObject>();
        Spline->SetObjectField(TEXT("position"), At);
        SplinePoints.Add(MakeShared<FJsonValueObject>(Spline));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("points"), Points);
    Result->SetNumberField(TEXT("lengthM"), RoadRound(Laid.LengthCm / 100.0, 10.0));
    Result->SetNumberField(TEXT("climbM"), RoadRound(Laid.ClimbCm / 100.0, 10.0));
    Result->SetNumberField(TEXT("maxGradePercent"), RoadRound(Laid.Grade * 100.0, 10.0));
    if (Laid.MinRadiusCm < TNumericLimits<double>::Max())
    {
        Result->SetNumberField(TEXT("minTurnRadiusM"), RoadRound(Laid.MinRadiusCm / 100.0, 10.0));
    }
    Result->SetNumberField(TEXT("switchbacks"), Laid.Switchbacks);
    Result->SetNumberField(TEXT("groundGapCm"), RoadRound(Laid.GapCm, 1.0));
    Result->SetNumberField(TEXT("cellCm"), RoadRound(Field.Cell, 1.0));
    Result->SetNumberField(TEXT("statesSearched"), Searched);
    Result->SetNumberField(TEXT("seconds"), RoadRound(FPlatformTime::Seconds() - Start, 100.0));
    TSharedPtr<FJsonObject> Road = MakeShared<FJsonObject>();
    Road->SetStringField(TEXT("name"), TEXT("Road"));
    Road->SetArrayField(TEXT("points"), SplinePoints);
    Road->SetNumberField(TEXT("width"), Width);
    Result->SetObjectField(TEXT("nextCall"), RoadCall(TEXT("create_road_spline"), Road));
    if (Landscape)
    {
        TSharedPtr<FJsonObject> Shaped = MakeShared<FJsonObject>();
        // A streaming proxy answers for the landscape it belongs to, the name configure_landscape_splines takes.
        const AActor* Owner = Landscape->GetLandscapeActor() ? static_cast<const AActor*>(Landscape->GetLandscapeActor()) : Landscape;
        Shaped->SetStringField(TEXT("landscapeName"), Owner->GetActorLabel());
        Shaped->SetArrayField(TEXT("points"), SplinePoints);
        Shaped->SetNumberField(TEXT("width"), Width);
        Result->SetObjectField(TEXT("landscapeSplineCall"), RoadCall(TEXT("configure_landscape_splines"), Shaped));
    }
    Bridge.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("A road of %.0f m climbing %.1f m, no grade over %.1f%%, %d switchback(s), through %d point(s). nextCall lays it as a road slab%s."),
                        Laid.LengthCm / 100.0, Laid.ClimbCm / 100.0, Laid.Grade * 100.0, Laid.Switchbacks, Route.Num(),
                        Landscape ? TEXT("; landscapeSplineCall makes it a landscape spline") : TEXT("")), Result);
    return true;
}
} // namespace McpRoadPlanning
