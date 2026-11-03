// McpGeometryAppendTubes.cpp — append_tubes: pipes, cables, rods and rings along a path of points, many in one call.
//
// A tube is a ring of `sides` points carried along its path by rotation-minimizing frames (double reflection, Wang et
// al. 2008), so a bend never twists it; at a joint the ring is widened across the turn (a mitre) so a corner keeps the
// tube's width. closed joins the last point back to the first (a ring, a torus, a loop of cable) and spreads the turn
// a frame picks up round the loop back along it; caps closes an open tube's ends with flat faces on points of their
// own, so the rims stay crisp. A tube's sides share one polygroup and each cap has its own. The faces go through the
// checks and the append that append_polygons runs (McpGeometryPolygonInput.cpp), like append_cuboids and append_loft.
#include "Domains/Geometry/McpGeometryHandlers.h"

#if MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
namespace
{
constexpr int32 MaxTubes = 500;
constexpr int32 MaxTubePathPoints = 256;
constexpr int32 MaxTubeVertices = 20000;
constexpr double MaxTubeRadius = 10000.0;
constexpr double MinTubeSegment = 0.01;
constexpr double MinTubeTurnCos = -0.866; // a turn past 150 degrees folds the tube back through itself
constexpr double MinTubeMitreCos = 0.25;
constexpr double TubeTwoPi = 6.283185307179586; // PI is gone in 5.8 and UE_PI missing in 5.0

struct FTubeSpec
{
    TArray<FVector3d> Path;
    double Radius = 0.0;
    double EndRadius = 0.0;
    int32 Sides = 12;
    int32 Material = 0;
    bool bClosed = false;
    bool bCaps = true;
};

// The direction from point I to the next one (the loop's first point after its last).
FVector3d TubeSegmentDirection(const FTubeSpec& Tube, int32 I)
{
    return (Tube.Path[(I + 1) % Tube.Path.Num()] - Tube.Path[I]).GetSafeNormal();
}

// Unit tangent at each point: the mean of the segments meeting there (the one segment at an open end).
TArray<FVector3d> TubeTangents(const FTubeSpec& Tube)
{
    const int32 N = Tube.Path.Num();
    TArray<FVector3d> Tangents;
    for (int32 I = 0; I < N; ++I)
    {
        const bool bOpenStart = I == 0 && !Tube.bClosed, bOpenEnd = I == N - 1 && !Tube.bClosed;
        const FVector3d In = bOpenStart ? FVector3d::ZeroVector : TubeSegmentDirection(Tube, (I + N - 1) % N);
        const FVector3d Out = bOpenEnd ? FVector3d::ZeroVector : TubeSegmentDirection(Tube, I);
        Tangents.Add((In + Out).GetSafeNormal());
    }
    return Tangents;
}

// One step of the double reflection method: the frame normal R carried from point A (tangent TA) to B (tangent TB).
FVector3d TubeTransport(const FVector3d& A, const FVector3d& B, const FVector3d& TA, const FVector3d& TB, const FVector3d& R)
{
    const FVector3d V1 = B - A;
    const double C1 = V1.Dot(V1);
    const FVector3d RL = R - (2.0 / C1) * V1.Dot(R) * V1;
    const FVector3d TL = TA - (2.0 / C1) * V1.Dot(TA) * V1;
    const FVector3d V2 = TB - TL;
    const double C2 = V2.Dot(V2);
    return (C2 > 1e-12 ? RL - (2.0 / C2) * V2.Dot(RL) * V2 : RL).GetSafeNormal();
}

// Rotation-minimizing frame normals, one per point, each perpendicular to its tangent.
TArray<FVector3d> TubeFrameNormals(const FTubeSpec& Tube, const TArray<FVector3d>& T)
{
    const int32 N = Tube.Path.Num();
    const FVector3d Up = FMath::Abs(T[0].Z) < 0.9 ? FVector3d(0.0, 0.0, 1.0) : FVector3d(1.0, 0.0, 0.0);
    TArray<FVector3d> R;
    R.Add(T[0].Cross(Up).GetSafeNormal());
    for (int32 I = 0; I + 1 < N; ++I)
    {
        R.Add(TubeTransport(Tube.Path[I], Tube.Path[I + 1], T[I], T[I + 1], R[I]));
    }
    if (Tube.bClosed)
    {
        // Carried once round the loop the frame comes back turned (a loop that is not flat has torsion): the turn is
        // spread back along the loop, so the seam between the last and the first ring has no step.
        const FVector3d Back = TubeTransport(Tube.Path[N - 1], Tube.Path[0], T[N - 1], T[0], R[N - 1]);
        const double Turn = FMath::Atan2(T[0].Dot(Back.Cross(R[0])), Back.Dot(R[0]));
        for (int32 I = 1; I < N; ++I)
        {
            R[I] = R[I].RotateAngleAxis(FMath::RadiansToDegrees(Turn * I / N), T[I]);
        }
    }
    return R;
}

bool ReadTubeSpec(const TSharedPtr<FJsonObject>& Object, int32 Index, FTubeSpec& Out, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    Out.bClosed = GetJsonBoolField(Object, TEXT("closed"), false);
    Out.bCaps = GetJsonBoolField(Object, TEXT("caps"), true);
    const int32 MinPoints = Out.bClosed ? 3 : 2;
    if (!Object->TryGetArrayField(TEXT("points"), Points) || Points->Num() < MinPoints || Points->Num() > MaxTubePathPoints)
    {
        OutError = FString::Printf(TEXT("tubes[%d].points must hold %d to %d {x, y, z} points along the tube"), Index, MinPoints, MaxTubePathPoints);
        return false;
    }
    for (int32 Point = 0; Point < Points->Num(); ++Point)
    {
        if (!ReadPointValue((*Points)[Point], Out.Path.AddDefaulted_GetRef()))
        {
            OutError = FString::Printf(TEXT("tubes[%d].points[%d] must be an {x, y, z} object of finite numbers"), Index, Point);
            return false;
        }
    }
    const int32 N = Out.Path.Num(), Segments = Out.bClosed ? N : N - 1;
    for (int32 I = 0; I < Segments; ++I)
    {
        if (FVector3d::Dist(Out.Path[I], Out.Path[(I + 1) % N]) < MinTubeSegment)
        {
            OutError = FString::Printf(TEXT("tubes[%d].points[%d] and the point after it are the same place: every segment needs a length"), Index, I);
            return false;
        }
    }
    for (int32 I = 0; I < Segments; ++I)
    {
        const bool bJoint = Out.bClosed || I + 1 < Segments;
        if (bJoint && TubeSegmentDirection(Out, I).Dot(TubeSegmentDirection(Out, (I + 1) % N)) < MinTubeTurnCos)
        {
            OutError = FString::Printf(TEXT("tubes[%d] turns back on itself at points[%d] (sharper than 150 degrees): add a point to round the turn"),
                                       Index, (I + 1) % N);
            return false;
        }
    }
    const bool bRadius = Object->TryGetNumberField(TEXT("radius"), Out.Radius);
    Out.EndRadius = Out.Radius;
    if (!bRadius || !(Out.Radius > 0.0 && Out.Radius <= MaxTubeRadius)
        || (Object->HasField(TEXT("endRadius")) && (!Object->TryGetNumberField(TEXT("endRadius"), Out.EndRadius) || !(Out.EndRadius > 0.0 && Out.EndRadius <= MaxTubeRadius)))
        || !ReadCount(Object, TEXT("sides"), 12, 3, 64, Out.Sides) || !ReadCount(Object, TEXT("materialId"), 0, 0, MAX_MATERIAL_ID, Out.Material))
    {
        OutError = FString::Printf(TEXT("tubes[%d] needs radius (and endRadius, when given) above 0 and at most %.0f cm, sides 3 to 64 and materialId 0 to %d"),
                                   Index, MaxTubeRadius, MAX_MATERIAL_ID);
        return false;
    }
    return true;
}

// Appends one tube moved by Shift: its rings, its side quads (one group) and its caps (a group each, own points).
void AddTubeFaces(FAppendPolygonsInput& Out, const FTubeSpec& Tube, const FVector3d& Shift, int32& InOutGroupKey)
{
    const int32 N = Tube.Path.Num(), S = Tube.Sides, Base = Out.Vertices.Num();
    const TArray<FVector3d> T = TubeTangents(Tube);
    const TArray<FVector3d> R = TubeFrameNormals(Tube, T);
    double Total = 0.0;
    TArray<double> Along = {0.0};
    for (int32 I = 1; I < N; ++I) Along.Add(Total += FVector3d::Dist(Tube.Path[I - 1], Tube.Path[I]));
    for (int32 I = 0; I < N; ++I)
    {
        const double Radius = FMath::Lerp(Tube.Radius, Tube.EndRadius, Total > 0.0 ? Along[I] / Total : 0.0);
        const FVector3d Side = T[I].Cross(R[I]);
        FVector3d Mitre = FVector3d::ZeroVector;
        double Widen = 1.0;
        if (Tube.bClosed || (I > 0 && I < N - 1))
        {
            const FVector3d D0 = TubeSegmentDirection(Tube, (I + N - 1) % N), D1 = TubeSegmentDirection(Tube, I);
            const double CosHalf = D0.Dot(T[I]);
            if (CosHalf < 0.9999)
            {
                Mitre = (D1 - D0).GetSafeNormal();
                Widen = 1.0 / FMath::Max(CosHalf, MinTubeMitreCos);
            }
        }
        for (int32 K = 0; K < S; ++K)
        {
            const double Phi = TubeTwoPi * K / S;
            FVector3d Offset = Radius * (FMath::Cos(Phi) * R[I] + FMath::Sin(Phi) * Side);
            Offset += (Widen - 1.0) * Offset.Dot(Mitre) * Mitre;
            Out.Vertices.Add(Tube.Path[I] + Shift + Offset);
        }
    }
    // Ring points run counter-clockwise about the tangent, so A0 A1 B1 B0 faces outward and the end ring's order faces on.
    const int32 SideKey = InOutGroupKey++;
    const int32 Segments = Tube.bClosed ? N : N - 1;
    for (int32 I = 0; I < Segments; ++I)
    {
        const int32 A = Base + I * S, B = Base + ((I + 1) % N) * S;
        for (int32 K = 0; K < S; ++K)
        {
            const int32 K1 = (K + 1) % S;
            Out.Faces.Add(TArray<int32>{A + K, A + K1, B + K1, B + K});
            Out.FaceMaterials.Add(Tube.Material);
            Out.FaceGroups.Add(SideKey);
        }
    }
    for (int32 End = 0; End < 2 && !Tube.bClosed && Tube.bCaps; ++End)
    {
        const int32 Ring = End == 0 ? Base : Base + (N - 1) * S, CapBase = Out.Vertices.Num();
        for (int32 K = 0; K < S; ++K)
        {
            const FVector3d RimPoint = Out.Vertices[Ring + K]; // a copy: Add must not read from the array it grows
            Out.Vertices.Add(RimPoint);
        }
        TArray<int32>& Cap = Out.Faces.AddDefaulted_GetRef();
        for (int32 K = 0; K < S; ++K) Cap.Add(CapBase + (End == 0 ? S - 1 - K : K));
        Out.FaceMaterials.Add(Tube.Material);
        Out.FaceGroups.Add(InOutGroupKey++);
    }
    ++Out.TubeCount;
}
} // namespace

bool ParseAppendTubesInput(const TSharedPtr<FJsonObject>& Payload, FAppendPolygonsInput& OutInput, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Tubes = nullptr;
    if (!Payload->TryGetArrayField(TEXT("tubes"), Tubes) || Tubes->Num() == 0 || Tubes->Num() > MaxTubes)
    {
        OutError = FString::Printf(TEXT("tubes required: 1 to %d tubes, each {points, radius} with optional endRadius, sides, closed, caps, materialId and repeat"), MaxTubes);
        return false;
    }
    OutInput.bFreshGroups = true;
    int32 GroupKey = 0;
    for (int32 Index = 0; Index < Tubes->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        FTubeSpec Tube;
        FMcpRepeatGrid Grid;
        if (!(*Tubes)[Index]->TryGetObject(Object) || !Object->IsValid())
        {
            OutError = FString::Printf(TEXT("tubes[%d] must be an object {points, radius}"), Index);
            return false;
        }
        if (!ReadTubeSpec(*Object, Index, Tube, OutError)) return false;
        if (!ReadRepeatGrid(*Object, Grid))
        {
            OutError = FString::Printf(TEXT("tubes[%d].repeat takes count, rows and layers of 1 or more and offsets {x, y, z}"), Index);
            return false;
        }
        const int64 Points = static_cast<int64>(Tube.Path.Num() + (Tube.bCaps && !Tube.bClosed ? 2 : 0)) * Tube.Sides * Grid.Num();
        if (OutInput.Vertices.Num() + Points > MaxTubeVertices)
        {
            OutError = FString::Printf(TEXT("the tubes come to more than %d points once their rings and repeats are laid out; append them in several calls or use fewer sides"),
                                       MaxTubeVertices);
            return false;
        }
        for (int32 Copy = 0; Copy < static_cast<int32>(Grid.Num()); ++Copy)
        {
            AddTubeFaces(OutInput, Tube, Grid.At(Copy), GroupKey);
        }
    }
    return FinishAppendPolygonsInput(OutInput, OutError);
}
} // namespace McpGeometryHandlers

#endif // MCP_HAS_FULL_GEOMETRY_SCRIPT
