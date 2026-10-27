// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// A road on a height grid, the part of plan_road that never touches the engine: cells of Cell cm, heights in cm, Open
// where a road may run. A* over each cell and the heading it was entered by, on 16 headings (the 8 neighbours and the 8
// knight moves, 26.57 or 18.43 degrees apart): an edge steeper than the grade limit anywhere along it, or crossing a
// closed cell, is no edge, and a turn is allowed only when its corner keeps the radius, min(|a|, |b|) / (2 tan(turn / 2)).
// Galin et al. 2010 (procedural roads: slope and curvature costs over a wide neighbourhood) is the reference design; the
// heading in the state is Hybrid A*'s idea, on a lattice.
namespace McpRoadPlan
{
struct FRoadField
{
    int32 Width = 0;
    int32 Height = 0;
    double Cell = 100.0;
    TArray<double> Heights;
    TArray<uint8> Open;

    bool IsOpen(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height && Open[Y * Width + X] != 0; }
    /** Bilinear, X and Y in cells. */
    double HeightAt(double X, double Y) const;
};

struct FRun
{
    bool bClear = false;
    double Grade = 0.0;  // the steepest rise over run along it
    double Gap = 0.0;    // how far the ground strays from the straight line between the two ends, cm
};

struct FMeasure
{
    double LengthCm = 0.0;
    double ClimbCm = 0.0;
    double Grade = 0.0;
    double MinRadiusCm = TNumericLimits<double>::Max();  // a straight road keeps the maximum
    double GapCm = 0.0;
    int32 Switchbacks = 0;
    bool bClear = true;
};

double CornerRadius(double LengthA, double LengthB, double Turn);
/** A straight run between two points in cells: clear of closed cells (sampled every quarter cell), and its grade; it stops
    early once the grade passes MaxGrade (the grade then says only that it is steeper). */
FRun Segment(const FRoadField& Field, const FVector2D& A, const FVector2D& B, double MaxGrade = TNumericLimits<double>::Max());
/** Closed cells grow by Cells (a disc), except within Cells of Keep: a road may end at a door. */
void Grow(FRoadField& Field, int32 Cells, const TArray<FIntPoint>& Keep);
/** The lattice path from From to To, empty when none: OutExpanded counts the states searched; bOutStopped says the deadline
    or the frontier's size limit ended the search, not the lack of a route. */
TArray<FIntPoint> Search(const FRoadField& Field, FIntPoint From, FIntPoint To, double MaxGrade, double RadiusCm, double Deadline,
                         int32& OutExpanded, bool& bOutStopped);
/** Lattice points merged where the run stays clear and under the grade and every corner keeps the radius. */
TArray<FIntPoint> Smooth(const FRoadField& Field, const TArray<FIntPoint>& Path, double MaxGrade, double RadiusCm);
FMeasure Measure(const FRoadField& Field, const TArray<FIntPoint>& Points);
} // namespace McpRoadPlan

namespace McpRoadPlanning
{
// build_environment plan_road (McpEnvRoad.cpp).
bool HandlePlanRoad(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpRoadPlanning
