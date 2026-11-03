// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Decals/McpDecalPlacement.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Engine/StaticMesh.h"
#include "Math/RandomStream.h"
#include "Math/RotationMatrix.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

namespace McpDecalPlacement
{
struct FDrawnMesh
{
    UE::Geometry::FDynamicMesh3 Mesh;
    TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> Tree;
};

FRotator SurfaceRotation(const FVector& Normal, double AngleDeg)
{
    const FVector Into = -Normal.GetSafeNormal();
    // A decal's texture runs down its +Y and right along its +Z (DeferredDecal.usf maps UV to its local Z and Y). It reads
    // upright from outside when +Y points down a wall, or toward -X on a floor or a ceiling (its top toward +X).
    const FVector Down = FMath::Abs(Into.Z) > 0.7 ? -FVector::ForwardVector : -FVector::UpVector;
    const FQuat Laid = FRotationMatrix::MakeFromXY(Into, Down).ToQuat();
    return (Laid * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(AngleDeg))).Rotator();
}

EDrawnHit DrawnHit(const FHitResult& Hit, const FVector& Start, const FVector& End, FDrawnMeshes& Meshes, FVector& OutPoint)
{
    // A spline mesh bends its mesh along the spline, so its triangles are not where the mesh's own space puts them.
    const UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Hit.GetComponent());
    UStaticMesh* Mesh = Component && !Component->IsA<USplineMeshComponent>() ? Component->GetStaticMesh() : nullptr;
    if (!Mesh) return EDrawnHit::NotAMesh;
    // ponytail: every static mesh a call hits is read once (about 0.6 s for 800k triangles); read only the meshes whose
    // collision is not their drawn triangles (simple as complex, Nanite, LODForCollision) if big Fab levels make calls slow.
    TSharedPtr<FDrawnMesh>* Found = Meshes.Find(Mesh);
    if (!Found)
    {
        TSharedPtr<FDrawnMesh> Drawn;
        // The source triangles, which LOD 0 and Nanite draw (a Nanite mesh's render data is its coarse fallback).
        if (const FMeshDescription* Description = Mesh->GetMeshDescription(0))
        {
            Drawn = MakeShared<FDrawnMesh>();
            FMeshDescriptionToDynamicMesh Converter;
            Converter.Convert(Description, Drawn->Mesh);
            Drawn->Tree = MakeUnique<UE::Geometry::FDynamicMeshAABBTree3>(&Drawn->Mesh, true);
        }
        Found = &Meshes.Add(Mesh, Drawn);
    }
    FTransform Transform = Component->GetComponentTransform();
    const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component);
    if (!Found->IsValid() || (Instanced && !Instanced->GetInstanceTransform(Hit.Item, Transform, true))) return EDrawnHit::NotAMesh;
    const FVector From = Transform.InverseTransformPosition(Start);
    const FVector Toward = Transform.InverseTransformPosition(End) - From;
    double Distance = 0.0;
    int TriangleId = -1;
    if (Toward.IsNearlyZero() || !(*Found)->Tree->FindNearestHitTriangle(FRay3d(From, Toward.GetSafeNormal()), Distance, TriangleId) ||
        Distance > Toward.Size())
    {
        return EDrawnHit::Missed;
    }
    OutPoint = Transform.TransformPosition(From + Toward.GetSafeNormal() * Distance);
    return EDrawnHit::Met;
}

TArray<FVector> ScatterInDisc(const FVector& Center, const FVector& Direction, double Radius, int32 Count, int32 Seed, double MinSpacing)
{
    constexpr double DecalTwoPi = 6.283185307179586; // PI is gone in 5.8 and UE_PI missing in 5.0
    // The disc faces the trace: level for a trace down or up, upright (across and up) for a trace into a wall.
    const bool bLevel = FMath::Abs(Direction.GetSafeNormal().Z) > 0.7;
    const FVector Across = bLevel ? FVector::ForwardVector : (Direction ^ FVector::UpVector).GetSafeNormal();
    const FVector Along = bLevel ? FVector::RightVector : FVector::UpVector;
    FRandomStream Random(Seed);
    TArray<FVector> Points;
    // Rejection sampling: a point too near one kept is drawn again, up to 30 draws a point.
    for (int32 Draw = 0; Points.Num() < Count && Draw < Count * 30; ++Draw)
    {
        const double Angle = DecalTwoPi * Random.FRand();
        const double Distance = Radius * FMath::Sqrt(static_cast<double>(Random.FRand()));
        const FVector Point = Center + Across * (FMath::Cos(Angle) * Distance) + Along * (FMath::Sin(Angle) * Distance);
        if (!Points.ContainsByPredicate([&](const FVector& Kept) { return FVector::Dist(Kept, Point) < MinSpacing; }))
        {
            Points.Add(Point);
        }
    }
    return Points;
}
} // namespace McpDecalPlacement
