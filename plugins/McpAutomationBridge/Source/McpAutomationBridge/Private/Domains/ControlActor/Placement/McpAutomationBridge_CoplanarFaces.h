// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class AActor;
class UWorld;

namespace McpCoplanar
{
// Two surfaces in one plane facing the same way: the depth test cannot order them, so where they overlap the
// picture flickers between the two (z-fighting). Pillars built flush with the ground block they stand in showed
// it all over a level while every other placement check passed.
struct FMcpCoplanarHit
{
    AActor* Actor = nullptr; // the smaller of the two pieces, the one to nudge
    AActor* OtherActor = nullptr;
    FString Component;
    FString OtherComponent;
    FVector Normal = FVector::ZeroVector; // the way both faces point
    double Gap = 0.0;                     // distance between the two planes
    double OverlapU = 0.0;
    double OverlapV = 0.0;
};

// Every coplanar face pair among the visible static mesh components of World, largest overlap first. Only faces a
// mesh really fills count (a cube's sides, a cylinder's caps), never the empty sides of a bounding box. With a
// NameFilter, only pairs where either actor's label contains it.
TArray<FMcpCoplanarHit> FindCoplanarFaces(UWorld* World, const FString& NameFilter);

// One actor's coplanar faces, ready for a placement report: its worst pair in words, every pair as data.
struct FMcpCoplanarReport
{
    AActor* Actor = nullptr;
    double Severity = 0.0; // side of the worst overlapping patch, in world units
    FString Issue;
    TArray<TSharedPtr<FJsonValue>> Faces;
};

// FindCoplanarFaces grouped by the actor to nudge, worst first.
TArray<FMcpCoplanarReport> ReportCoplanarFaces(UWorld* World, const FString& NameFilter);
} // namespace McpCoplanar
