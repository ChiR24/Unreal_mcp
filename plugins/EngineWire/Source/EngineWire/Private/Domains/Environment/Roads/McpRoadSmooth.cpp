// Copyright (c) 2024 EngineWire Contributors
//
// Straightening a searched road and measuring it as laid (McpRoadPlan.h).

#include "Domains/Environment/Roads/McpRoadPlan.h"

namespace McpRoadPlan
{
namespace
{
// A merge may leave the straight road at most this far from the ground between its ends: a long run across a convex
// hillside cut 1.15 m into it.
constexpr double RoadFitCm = 25.0;

double RoadLength(const FIntPoint& A, const FIntPoint& B)
{
    return FVector2D::Distance(FVector2D(A), FVector2D(B));
}

// The turn at B on the way A -> B -> C, in radians.
double RoadTurnAt(const FIntPoint& A, const FIntPoint& B, const FIntPoint& C)
{
    const double In = FMath::Atan2(static_cast<double>(B.Y - A.Y), static_cast<double>(B.X - A.X));
    const double Out = FMath::Atan2(static_cast<double>(C.Y - B.Y), static_cast<double>(C.X - B.X));
    const double Turn = FMath::Abs(Out - In);
    return Turn > 3.14159265358979323846 ? 2.0 * 3.14159265358979323846 - Turn : Turn;
}

bool RoadCornerHolds(const FRoadField& Field, const FIntPoint& A, const FIntPoint& B, const FIntPoint& C, double RadiusCm)
{
    return CornerRadius(RoadLength(A, B) * Field.Cell, RoadLength(B, C) * Field.Cell, RoadTurnAt(A, B, C)) >= RadiusCm;
}
} // namespace

TArray<FIntPoint> Smooth(const FRoadField& Field, const TArray<FIntPoint>& Path, double MaxGrade, double RadiusCm)
{
    TArray<FIntPoint> Kept;
    if (Path.Num() == 0)
    {
        return Kept;
    }
    Kept.Add(Path[0]);
    // From each kept point, the farthest lattice point whose run is clear and under the grade, whose corner here holds
    // the radius, and whose corner there holds it with the lattice edge after it: that edge then always remains a
    // valid next step, so a merge never strands the road at a corner it cannot turn.
    for (int32 From = 0; From < Path.Num() - 1;)
    {
        int32 Next = From + 1;
        // At most 64 lattice steps ahead: each try samples its whole run, so an unbounded look made long roads quadratic.
        for (int32 To = FMath::Min(Path.Num() - 1, From + 64); To > From + 1; --To)
        {
            const FRun Run = Segment(Field, FVector2D(Path[From]), FVector2D(Path[To]), MaxGrade);
            if (!Run.bClear || Run.Grade > MaxGrade || Run.Gap > RoadFitCm)
            {
                continue;
            }
            if (Kept.Num() > 1 && !RoadCornerHolds(Field, Kept[Kept.Num() - 2], Path[From], Path[To], RadiusCm))
            {
                continue;
            }
            if (To < Path.Num() - 1 && !RoadCornerHolds(Field, Path[From], Path[To], Path[To + 1], RadiusCm))
            {
                continue;
            }
            Next = To;
            break;
        }
        Kept.Add(Path[Next]);
        From = Next;
    }
    return Kept;
}

FMeasure Measure(const FRoadField& Field, const TArray<FIntPoint>& Points)
{
    FMeasure Out;
    double Reference = 0.0;
    bool bHasReference = false;
    for (int32 Index = 1; Index < Points.Num(); ++Index)
    {
        const FIntPoint& A = Points[Index - 1];
        const FIntPoint& B = Points[Index];
        const FRun Run = Segment(Field, FVector2D(A), FVector2D(B));
        Out.Grade = FMath::Max(Out.Grade, Run.Grade);
        Out.GapCm = FMath::Max(Out.GapCm, Run.Gap);
        Out.bClear &= Run.bClear;
        Out.LengthCm += RoadLength(A, B) * Field.Cell;
        Out.ClimbCm += FMath::Max(0.0, Field.HeightAt(B.X, B.Y) - Field.HeightAt(A.X, A.Y));
        if (Index < Points.Num() - 1)
        {
            Out.MinRadiusCm = FMath::Min(Out.MinRadiusCm, CornerRadius(RoadLength(A, B) * Field.Cell, RoadLength(B, Points[Index + 1]) * Field.Cell,
                                                                        RoadTurnAt(A, B, Points[Index + 1])));
        }
        // A switchback: the heading has turned by more than 90 degrees since the last one.
        const double Heading = FMath::Atan2(static_cast<double>(B.Y - A.Y), static_cast<double>(B.X - A.X));
        if (!bHasReference)
        {
            Reference = Heading;
            bHasReference = true;
            continue;
        }
        double Turned = FMath::Abs(Heading - Reference);
        Turned = Turned > 3.14159265358979323846 ? 2.0 * 3.14159265358979323846 - Turned : Turned;
        if (Turned > 3.14159265358979323846 / 2.0)
        {
            ++Out.Switchbacks;
            Reference = Heading;
        }
    }
    return Out;
}
} // namespace McpRoadPlan
