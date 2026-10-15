// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
namespace
{
// Planes closer than this are one plane to the depth buffer at play distance.
constexpr double McpCoplanarPlaneTolerance = 0.5;
// An overlap thinner than this is two faces meeting along an edge, not a patch anyone sees.
constexpr double McpCoplanarMinOverlap = 1.0;

struct FMcpCoplanarFace
{
    FVector Center = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    FVector U = FVector::ZeroVector;
    FVector V = FVector::ZeroVector;
    double HalfU = 0.0;
    double HalfV = 0.0;
};

struct FMcpCoplanarPiece
{
    AActor* Actor = nullptr;
    const UStaticMeshComponent* Component = nullptr;
    FBox Bounds = FBox(ForceInit);
    double Volume = 0.0;
    bool bNamed = true;
    TArray<FMcpCoplanarFace, TInlineAllocator<6>> Faces;
};

// The world-space faces a component's mesh really fills.
void McpCoplanarAddFaces(const UStaticMeshComponent* Component, const FMcpCoplanarFill& Fill, FMcpCoplanarPiece& Piece)
{
    const FBox Box = Component->GetStaticMesh()->GetBoundingBox();
    const FVector Extent = Box.GetExtent();
    const FTransform& Transform = Component->GetComponentTransform();
    for (int32 Side = 0; Side < 6; ++Side)
    {
        if (Fill.Share[Side] < 0.5)
        {
            continue;
        }
        const int32 Axis = Side / 2;
        const int32 AxisU = (Axis + 1) % 3;
        const int32 AxisV = (Axis + 2) % 3;
        FVector LocalCenter = Box.GetCenter();
        LocalCenter[Axis] = (Side % 2) ? Box.Max[Axis] : Box.Min[Axis];
        FVector LocalNormal = FVector::ZeroVector;
        LocalNormal[Axis] = (Side % 2) ? 1.0 : -1.0;
        FVector LocalU = FVector::ZeroVector;
        LocalU[AxisU] = 1.0;
        FVector LocalV = FVector::ZeroVector;
        LocalV[AxisV] = 1.0;
        const FVector WorldU = Transform.TransformVector(LocalU);
        const FVector WorldV = Transform.TransformVector(LocalV);
        FMcpCoplanarFace& Face = Piece.Faces.AddDefaulted_GetRef();
        Face.Center = Transform.TransformPosition(LocalCenter);
        // A negative scale mirrors the side, and TransformVector carries that sign into the normal.
        Face.Normal = Transform.TransformVector(LocalNormal).GetSafeNormal();
        Face.U = WorldU.GetSafeNormal();
        Face.V = WorldV.GetSafeNormal();
        Face.HalfU = Extent[AxisU] * WorldU.Size();
        Face.HalfV = Extent[AxisV] * WorldV.Size();
    }
}

// Whether B lies in A's plane, faces the same way and overlaps it by more than an edge.
bool McpCoplanarOverlap(const FMcpCoplanarFace& A, const FMcpCoplanarFace& B, double& OutGap, double& OutU, double& OutV)
{
    if (FVector::DotProduct(A.Normal, B.Normal) < 0.9995)
    {
        return false;
    }
    const FVector Delta = B.Center - A.Center;
    OutGap = FMath::Abs(FVector::DotProduct(Delta, A.Normal));
    if (OutGap > McpCoplanarPlaneTolerance)
    {
        return false;
    }
    // B's reach along A's axes: exact when the two rectangles share axes, a bound when B is turned in the plane.
    const double ReachU = FMath::Abs(FVector::DotProduct(B.U, A.U)) * B.HalfU + FMath::Abs(FVector::DotProduct(B.V, A.U)) * B.HalfV;
    const double ReachV = FMath::Abs(FVector::DotProduct(B.U, A.V)) * B.HalfU + FMath::Abs(FVector::DotProduct(B.V, A.V)) * B.HalfV;
    const double CenterU = FVector::DotProduct(Delta, A.U);
    const double CenterV = FVector::DotProduct(Delta, A.V);
    OutU = FMath::Min(A.HalfU, CenterU + ReachU) - FMath::Max(-A.HalfU, CenterU - ReachU);
    OutV = FMath::Min(A.HalfV, CenterV + ReachV) - FMath::Max(-A.HalfV, CenterV - ReachV);
    return OutU > McpCoplanarMinOverlap && OutV > McpCoplanarMinOverlap;
}

// A mesh kept out of the main view cannot flicker in it: one drawn outside the main pass, and a water body's two info
// meshes, which its own scene proxy hides from every pass but the water info texture's (an ocean was reported as one
// coplanar pair of them). Matched by class name so the plugin needs no link to the Water module.
bool McpCoplanarDrawnInView(const UStaticMeshComponent* Component)
{
    return Component->bRenderInMainPass && Component->GetClass()->GetFName() != TEXT("WaterBodyInfoMeshComponent");
}

TArray<FMcpCoplanarPiece> McpCoplanarCollect(UWorld* World, const FString& NameFilter, const TArray<AActor*>* Focus)
{
    TArray<FMcpCoplanarPiece> Pieces;
    TMap<const UStaticMesh*, FMcpCoplanarFill> Fills;
    // Reading a mesh's triangles is the cost, so with a focus only pieces that can meet a focus actor are read.
    TArray<FBox> Near;
    if (Focus)
    {
        for (const AActor* Actor : *Focus)
        {
            Near.Add(Actor->GetComponentsBoundingBox(true).ExpandBy(McpCoplanarPlaneTolerance + 1.0));
        }
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor->IsHidden())
        {
            continue;
        }
        // Also the object name: actors sharing a label are reported by it.
        const bool bNamed = Focus ? Focus->Contains(Actor)
                                  : NameFilter.IsEmpty() || Actor->GetActorLabel().Contains(NameFilter) ||
                                        Actor->GetName().Contains(NameFilter);
        TArray<UStaticMeshComponent*> Components;
        Actor->GetComponents<UStaticMeshComponent>(Components);
        for (const UStaticMeshComponent* Component : Components)
        {
            // Instanced components hold many transforms under one; their bounds are not one mesh.
            if (!Component || !Component->IsVisible() || Component->bHiddenInGame || !Component->GetStaticMesh() ||
                Component->IsA<UInstancedStaticMeshComponent>() || !McpCoplanarDrawnInView(Component))
            {
                continue;
            }
            const FBox Bounds = Component->Bounds.GetBox();
            if (Focus && !Near.ContainsByPredicate([&Bounds](const FBox& Box) { return Box.Intersect(Bounds); }))
            {
                continue;
            }
            const UStaticMesh* Mesh = Component->GetStaticMesh();
            const FMcpCoplanarFill* Fill = Fills.Find(Mesh);
            if (!Fill)
            {
                Fill = &Fills.Add(Mesh, MeasureFill(Mesh));
            }
            FMcpCoplanarPiece Piece;
            Piece.Actor = Actor;
            Piece.Component = Component;
            Piece.Bounds = Bounds;
            Piece.Volume = Piece.Bounds.GetVolume();
            Piece.bNamed = bNamed;
            McpCoplanarAddFaces(Component, *Fill, Piece);
            if (Piece.Faces.Num() > 0)
            {
                Pieces.Add(MoveTemp(Piece));
            }
        }
    }
    return Pieces;
}
} // namespace

TArray<FMcpCoplanarHit> FindCoplanarFaces(UWorld* World, const FString& NameFilter, const TArray<AActor*>* Focus)
{
    TArray<FMcpCoplanarHit> Hits;
    if (!World)
    {
        return Hits;
    }
    TArray<FMcpCoplanarPiece> Pieces = McpCoplanarCollect(World, NameFilter, Focus);
    // Sweep along X: only pieces whose boxes meet can share a face.
    Pieces.Sort([](const FMcpCoplanarPiece& A, const FMcpCoplanarPiece& B) { return A.Bounds.Min.X < B.Bounds.Min.X; });
    const FVector Slack(McpCoplanarPlaneTolerance);
    for (int32 I = 0; I < Pieces.Num(); ++I)
    {
        const FMcpCoplanarPiece& A = Pieces[I];
        const FBox Reach(A.Bounds.Min - Slack, A.Bounds.Max + Slack);
        for (int32 J = I + 1; J < Pieces.Num() && Pieces[J].Bounds.Min.X <= Reach.Max.X; ++J)
        {
            const FMcpCoplanarPiece& B = Pieces[J];
            if ((!A.bNamed && !B.bNamed) || !Reach.Intersect(B.Bounds))
            {
                continue;
            }
            FMcpCoplanarHit Best;
            double AreaA = 0.0;
            double AreaB = 0.0;
            for (const FMcpCoplanarFace& FaceA : A.Faces)
            {
                for (const FMcpCoplanarFace& FaceB : B.Faces)
                {
                    double Gap = 0.0;
                    double U = 0.0;
                    double V = 0.0;
                    if (McpCoplanarOverlap(FaceA, FaceB, Gap, U, V) && U * V > Best.OverlapU * Best.OverlapV)
                    {
                        Best.Normal = FaceA.Normal;
                        Best.Gap = Gap;
                        Best.OverlapU = U;
                        Best.OverlapV = V;
                        AreaA = 4.0 * FaceA.HalfU * FaceA.HalfV;
                        AreaB = 4.0 * FaceB.HalfU * FaceB.HalfV;
                    }
                }
            }
            if (Best.OverlapU <= 0.0)
            {
                continue;
            }
            // The smaller piece is the one to move: nudging a pillar beats shifting the ground under the level.
            const bool bASmaller = A.Volume <= B.Volume;
            const FMcpCoplanarPiece& Subject = bASmaller ? A : B;
            const FMcpCoplanarPiece& Other = bASmaller ? B : A;
            Best.Actor = Subject.Actor;
            Best.OtherActor = Other.Actor;
            Best.Component = Subject.Component->GetName();
            Best.OtherComponent = Other.Component->GetName();
            Best.FaceArea = bASmaller ? AreaA : AreaB;
            Hits.Add(Best);
        }
    }
    Hits.Sort([](const FMcpCoplanarHit& A, const FMcpCoplanarHit& B) { return A.OverlapU * A.OverlapV > B.OverlapU * B.OverlapV; });
    return Hits;
}
} // namespace McpCoplanar
