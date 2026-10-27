// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Geometry/Roof/McpRoofOutline.h"

#include "Algo/Reverse.h"

namespace McpRoofOutline
{
namespace
{
constexpr double OutlineMerge = 0.5; // cm: corners this close are one

double OutlineCross(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

double OutlineArea(const TArray<FVector2D>& Outline)
{
    double Area = 0.0;
    for (int32 I = 0; I < Outline.Num(); ++I)
    {
        Area += OutlineCross(Outline[I], Outline[(I + 1) % Outline.Num()]);
    }
    return Area / 2.0;
}

// C lies on segment AB, given D, the cross product that says it is on AB's line.
bool OutlineOnSegment(const FVector2D& A, const FVector2D& B, const FVector2D& C, double D)
{
    return FMath::Abs(D) <= 1e-9 * FMath::Max(1.0, (B - A).SizeSquared()) && FVector2D::DotProduct(C - A, C - B) <= 0.0;
}

// Whether segments PQ and RS share a point.
bool OutlineSegmentsMeet(const FVector2D& P, const FVector2D& Q, const FVector2D& R, const FVector2D& S)
{
    const double D1 = OutlineCross(Q - P, R - P), D2 = OutlineCross(Q - P, S - P);
    const double D3 = OutlineCross(S - R, P - R), D4 = OutlineCross(S - R, Q - R);
    if (((D1 > 0.0 && D2 < 0.0) || (D1 < 0.0 && D2 > 0.0)) && ((D3 > 0.0 && D4 < 0.0) || (D3 < 0.0 && D4 > 0.0)))
    {
        return true;
    }
    return OutlineOnSegment(P, Q, R, D1) || OutlineOnSegment(P, Q, S, D2) || OutlineOnSegment(R, S, P, D3) || OutlineOnSegment(R, S, Q, D4);
}

int32 OutlineRoot(TArray<int32>& Root, int32 I)
{
    while (Root[I] != I)
    {
        Root[I] = Root[Root[I]];
        I = Root[I];
    }
    return I;
}
} // namespace

TArray<FVector2D> Clean(const TArray<FVector2D>& Outline)
{
    TArray<FVector2D> Merged;
    for (const FVector2D& Point : Outline)
    {
        if (Merged.Num() == 0 || FVector2D::Distance(Merged.Last(), Point) >= OutlineMerge)
        {
            Merged.Add(Point);
        }
    }
    while (Merged.Num() > 1 && FVector2D::Distance(Merged[0], Merged.Last()) < OutlineMerge)
    {
        Merged.Pop();
    }
    if (OutlineArea(Merged) < 0.0)
    {
        Algo::Reverse(Merged);
    }
    TArray<FVector2D> Out;
    for (int32 I = 0; I < Merged.Num(); ++I)
    {
        // A corner in a straight line with its neighbours (or folding back on itself) is no corner.
        const FVector2D In = Merged[I] - Merged[(I + Merged.Num() - 1) % Merged.Num()];
        const FVector2D Next = Merged[(I + 1) % Merged.Num()] - Merged[I];
        if (FMath::Abs(OutlineCross(In, Next)) > 1e-9 * In.Size() * Next.Size())
        {
            Out.Add(Merged[I]);
        }
    }
    return Out;
}

TArray<FVector2D> Offset(const TArray<FVector2D>& Outline, double Distance)
{
    const int32 N = Outline.Num();
    TArray<FVector2D> Normal;
    TArray<double> Line;
    for (int32 I = 0; I < N; ++I)
    {
        const FVector2D Dir = (Outline[(I + 1) % N] - Outline[I]).GetSafeNormal();
        Normal.Add(FVector2D(-Dir.Y, Dir.X)); // inward for a counter-clockwise outline
        Line.Add(FVector2D::DotProduct(Normal[I], Outline[I]) - Distance);
    }
    TArray<FVector2D> Out;
    for (int32 I = 0; I < N; ++I)
    {
        const int32 P = (I + N - 1) % N;
        const double Det = Normal[P].X * Normal[I].Y - Normal[P].Y * Normal[I].X;
        Out.Add(FMath::Abs(Det) < 1e-12 ? Outline[I] - Normal[I] * Distance
                                        : FVector2D((Line[P] * Normal[I].Y - Normal[P].Y * Line[I]) / Det, (Normal[P].X * Line[I] - Line[P] * Normal[I].X) / Det));
    }
    return Out;
}

bool CrossesItself(const TArray<FVector2D>& Outline)
{
    const int32 N = Outline.Num();
    for (int32 I = 0; I < N; ++I)
    {
        for (int32 J = I + 2; J < N; ++J)
        {
            if (!(I == 0 && J == N - 1) && OutlineSegmentsMeet(Outline[I], Outline[(I + 1) % N], Outline[J], Outline[(J + 1) % N]))
            {
                return true;
            }
        }
    }
    return false;
}

bool Faces(const TArray<McpRoofSkeleton::FNode>& Nodes, const TArray<FArc>& Arcs, int32 EdgeCount, double Weld,
           TArray<McpRoofSkeleton::FNode>& OutNodes, TArray<TArray<int32>>& OutFaces)
{
    TArray<int32> Root;
    for (int32 I = 0; I < Nodes.Num(); ++I)
    {
        Root.Add(I);
        for (int32 J = 0; J < I; ++J)
        {
            const int32 A = OutlineRoot(Root, I), B = OutlineRoot(Root, J);
            if (A != B && FVector2D::Distance(Nodes[I].Point, Nodes[J].Point) < Weld)
            {
                Root[FMath::Max(A, B)] = FMath::Min(A, B); // the lowest index leads, so the corners keep theirs
            }
        }
    }
    TArray<int32> Map;
    Map.Init(INDEX_NONE, Nodes.Num());
    for (int32 I = 0; I < Nodes.Num(); ++I)
    {
        const int32 Lead = OutlineRoot(Root, I);
        if (I < EdgeCount && Lead != I)
        {
            return false; // two corners welded
        }
        Map[I] = Map[Lead] != INDEX_NONE ? Map[Lead] : (Map[Lead] = OutNodes.Add(Nodes[Lead]));
    }
    for (int32 Edge = 0; Edge < EdgeCount; ++Edge)
    {
        TArray<TPair<int32, int32>> Tagged;
        for (const FArc& Arc : Arcs)
        {
            if ((Arc.EdgeIn == Edge || Arc.EdgeOut == Edge) && Map[Arc.A] != Map[Arc.B])
            {
                Tagged.Add({Map[Arc.A], Map[Arc.B]});
            }
        }
        // From the edge's end corner back along the arcs traced on it to its start corner.
        TArray<int32> Face = {Edge, (Edge + 1) % EdgeCount};
        TArray<bool> Used;
        Used.Init(false, Tagged.Num());
        int32 Cur = Face[1];
        for (int32 Step = 0; Step < Tagged.Num() + 2 && Cur != Edge; ++Step)
        {
            int32 K = 0;
            while (K < Tagged.Num() && (Used[K] || (Tagged[K].Key != Cur && Tagged[K].Value != Cur)))
            {
                ++K;
            }
            if (K == Tagged.Num())
            {
                break;
            }
            Used[K] = true;
            Cur = Tagged[K].Key == Cur ? Tagged[K].Value : Tagged[K].Key;
            if (Cur != Edge)
            {
                Face.Add(Cur);
            }
        }
        if (Cur != Edge)
        {
            return false;
        }
        OutFaces.Add(MoveTemp(Face));
    }
    return true;
}
} // namespace McpRoofOutline
