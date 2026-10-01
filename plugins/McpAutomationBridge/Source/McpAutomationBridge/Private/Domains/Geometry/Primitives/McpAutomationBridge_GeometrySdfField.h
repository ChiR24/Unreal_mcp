#pragma once

#include "CoreMinimal.h"

// The signed distance field behind create_sdf: shapes in their own frames, combined in
// order with smooth union, subtract and intersect (Inigo Quilez's polynomial smooth min).
namespace McpGeometrySdf
{
enum class EShape : uint8 { Sphere, Ellipsoid, Box, Capsule, Cylinder, Torus, Cone };
enum class EOp : uint8 { Union, Subtract, Intersect };

struct FShape
{
    EShape Type = EShape::Sphere;
    EOp Op = EOp::Union;
    FTransform Frame;
    FVector3d Radii = FVector3d(50.0);
    FVector3d Extent = FVector3d(50.0);
    double Radius = 50.0;
    double TopRadius = 0.0;
    double Length = 100.0;
    double Rounding = 0.0;
    double Thickness = 10.0;
    double Blend = 0.0;
    int32 MaterialId = 0;
};

inline double Len2(double X, double Y) { return FMath::Sqrt(X * X + Y * Y); }

// Exact or bound-preserving distances, P in the shape's own frame. Capsule, cylinder
// and cone run along local Z, centred; the torus lies in local XY.
inline double ShapeDistance(const FShape& S, const FVector3d& P)
{
    switch (S.Type)
    {
    case EShape::Sphere: return P.Length() - S.Radius;
    case EShape::Ellipsoid:
    {
        const double K0 = (P / S.Radii).Length();
        const double K1 = (P / (S.Radii * S.Radii)).Length();
        return K1 > 1e-12 ? K0 * (K0 - 1.0) / K1 : -S.Radii.GetMin();
    }
    case EShape::Box:
    {
        const double R = FMath::Min(S.Rounding, S.Extent.GetMin());
        const FVector3d Q = P.GetAbs() - (S.Extent - FVector3d(R));
        return FVector3d::Max(Q, FVector3d::ZeroVector).Length() + FMath::Min(Q.GetMax(), 0.0) - R;
    }
    case EShape::Capsule:
    {
        const double H = S.Length * 0.5;
        return FVector3d(P.X, P.Y, P.Z - FMath::Clamp(P.Z, -H, H)).Length() - S.Radius;
    }
    case EShape::Cylinder:
    {
        const double R = FMath::Min(S.Rounding, FMath::Min(S.Radius, S.Length * 0.5));
        const double DX = Len2(P.X, P.Y) - S.Radius + R;
        const double DZ = FMath::Abs(P.Z) - S.Length * 0.5 + R;
        return FMath::Min(FMath::Max(DX, DZ), 0.0) + Len2(FMath::Max(DX, 0.0), FMath::Max(DZ, 0.0)) - R;
    }
    case EShape::Torus: return Len2(Len2(P.X, P.Y) - S.Radius, P.Z) - S.Thickness;
    default:
    {
        // Round cone: Radius at z = -Length/2 tapering to TopRadius at z = +Length/2.
        const double H = FMath::Max(S.Length, 1e-6);
        const double QX = Len2(P.X, P.Y);
        const double QY = P.Z + H * 0.5;
        const double B = FMath::Clamp((S.Radius - S.TopRadius) / H, -0.999, 0.999);
        const double A = FMath::Sqrt(1.0 - B * B);
        const double K = -B * QX + A * QY;
        if (K < 0.0) return Len2(QX, QY) - S.Radius;
        if (K > A * H) return Len2(QX, QY - H) - S.TopRadius;
        return A * QX + B * QY - S.Radius;
    }
    }
}

inline double LocalDistance(const FShape& S, const FVector3d& Pt)
{
    return ShapeDistance(S, S.Frame.InverseTransformPosition(Pt));
}

// Shapes apply in order onto the first; each Blend is the smooth-min radius of its join.
// OutOwner receives the shape whose field decides the surface at Pt: a union where it is
// the nearer surface, a subtract where its carve wins, an intersect where it is the tighter
// bound. That switches on the centre line of each fillet, so a part's material ends there
// instead of wherever another shape's surface happens to pass close by.
inline double FieldDistance(const TArray<FShape>& Shapes, const FVector3d& Pt, int32* OutOwner = nullptr)
{
    double D = LocalDistance(Shapes[0], Pt);
    int32 Owner = 0;
    for (int32 Index = 1; Index < Shapes.Num(); ++Index)
    {
        const FShape& S = Shapes[Index];
        const double Ds = LocalDistance(S, Pt);
        const double K = FMath::Max(S.Blend, 1e-6);
        if (S.Op == EOp::Union)
        {
            const double H = FMath::Clamp(0.5 + 0.5 * (D - Ds) / K, 0.0, 1.0);
            if (Ds < D) Owner = Index;
            D = FMath::Lerp(D, Ds, H) - K * H * (1.0 - H);
        }
        else if (S.Op == EOp::Subtract)
        {
            const double H = FMath::Clamp(0.5 - 0.5 * (D + Ds) / K, 0.0, 1.0);
            if (-Ds > D) Owner = Index;
            D = FMath::Lerp(D, -Ds, H) + K * H * (1.0 - H);
        }
        else
        {
            const double H = FMath::Clamp(0.5 - 0.5 * (Ds - D) / K, 0.0, 1.0);
            if (Ds > D) Owner = Index;
            D = FMath::Lerp(Ds, D, H) + K * H * (1.0 - H);
        }
    }
    if (OutOwner) *OutOwner = Owner;
    return D;
}

inline int32 FieldOwner(const TArray<FShape>& Shapes, const FVector3d& Pt)
{
    int32 Owner = 0;
    FieldDistance(Shapes, Pt, &Owner);
    return Owner;
}

inline double BoundRadius(const FShape& S)
{
    switch (S.Type)
    {
    case EShape::Sphere: return S.Radius;
    case EShape::Ellipsoid: return S.Radii.GetMax();
    case EShape::Box: return S.Extent.Length();
    case EShape::Capsule: return S.Radius + S.Length * 0.5;
    case EShape::Cylinder: return Len2(S.Radius, S.Length * 0.5);
    case EShape::Torus: return S.Radius + S.Thickness;
    default: return Len2(FMath::Max(S.Radius, S.TopRadius), S.Length * 0.5);
    }
}
} // namespace McpGeometrySdf
