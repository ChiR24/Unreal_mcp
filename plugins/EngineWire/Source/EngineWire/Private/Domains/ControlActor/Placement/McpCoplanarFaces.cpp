// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
namespace
{
// An overlap thinner than this is two faces meeting along an edge, not a patch anyone sees.
constexpr double McpCoplanarMinOverlap = 1.0;

// A mesh kept out of the main view cannot flicker in it: one drawn outside the main pass, and a water body's two info
// meshes, which its own scene proxy hides from every pass but the water info texture's (an ocean was reported as one
// coplanar pair of them). Matched by class name so the plugin needs no link to the Water module.
bool McpCoplanarDrawnInView(const UStaticMeshComponent* Component)
{
    return Component->bRenderInMainPass && Component->GetClass()->GetFName() != TEXT("WaterBodyInfoMeshComponent");
}

// Two undersides with something solid right below them are seen from nowhere: carpet trims whose bottoms lay in the
// carpet's plane on the floor were reported, and fixed, as flicker no camera could show.
bool McpCoplanarHiddenUnderside(UWorld* World, const FVector& Normal, const FVector& Center, const AActor* A, const AActor* B)
{
    if (Normal.Z > -0.99)
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(McpCoplanarUnderside), true);
    Params.AddIgnoredActor(A);
    Params.AddIgnoredActor(B);
    FHitResult Hit;
    return World->LineTraceSingleByChannel(Hit, Center + FVector(0.0, 0.0, 0.5), Center - FVector(0.0, 0.0, 2.0), ECC_Visibility, Params);
}
} // namespace

void AddBoxFaces(const FBox& Box, const FTransform& Transform, const FMcpCoplanarFill& Fill, FMcpCoplanarPiece& Piece)
{
    const FVector Extent = Box.GetExtent();
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

bool FacesInOnePlane(const FMcpCoplanarFace& A, const FMcpCoplanarFace& B, double& OutGap, double& OutU, double& OutV)
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

TArray<FMcpCoplanarPiece> CollectPieces(UWorld* World, const FString& NameFilter, const TArray<AActor*>* Focus)
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
    int32 Owner = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor->IsHidden())
        {
            continue;
        }
        ++Owner;
        // Also the object name: actors sharing a label are reported by it.
        const bool bNamed = Focus ? Focus->Contains(Actor)
                                  : NameFilter.IsEmpty() || Actor->GetActorLabel().Contains(NameFilter) ||
                                        Actor->GetName().Contains(NameFilter);
        TArray<UStaticMeshComponent*> Components;
        Actor->GetComponents<UStaticMeshComponent>(Components);
        const int32 First = Pieces.Num();
        for (const UStaticMeshComponent* Component : Components)
        {
            // Instanced components hold many transforms under one; their bounds are not one mesh. A spline mesh bends its
            // mesh along a curve, so the mesh's box under its transform is not where its faces are: every segment of a
            // road read as the same stock cube at the actor's origin, z-fighting with the next.
            // ponytail: a straight spline segment flush with a slab goes unchecked; take its faces from the sections at
            // its two ends (CalcSliceTransform) if that flicker needs catching.
            if (!Component || !Component->IsVisible() || Component->bHiddenInGame || !Component->GetStaticMesh() ||
                Component->IsA<UInstancedStaticMeshComponent>() || Component->IsA<USplineMeshComponent>() ||
                !McpCoplanarDrawnInView(Component))
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
            Piece.Owner = Owner;
            Piece.Bounds = Bounds;
            Piece.Volume = Piece.Bounds.GetVolume();
            Piece.bNamed = bNamed;
            AddBoxFaces(Mesh->GetBoundingBox(), Component->GetComponentTransform(), *Fill, Piece);
            if (Piece.Faces.Num() > 0)
            {
                Pieces.Add(MoveTemp(Piece));
            }
        }
        for (int32 Part = First; Part < Pieces.Num(); ++Part)
        {
            for (int32 Other = First; Other < Pieces.Num(); ++Other)
            {
                if (Other != Part && Pieces[Other].Component->IsAttachedTo(Pieces[Part].Component))
                {
                    Pieces[Part].Carried.Add(Other);
                }
            }
        }
    }
    return Pieces;
}

TArray<FMcpCoplanarPair> FindPairs(const TArray<FMcpCoplanarPiece>& Pieces, bool bBestOnly)
{
    TArray<FMcpCoplanarPair> Pairs;
    // Sweep along X: only pieces whose boxes meet can share a face. A stable order keeps equal pieces as they came.
    TArray<int32> Order;
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        Order.Add(Index);
    }
    Order.StableSort([&Pieces](int32 A, int32 B) { return Pieces[A].Bounds.Min.X < Pieces[B].Bounds.Min.X; });
    const FVector Slack(McpCoplanarPlaneTolerance);
    for (int32 I = 0; I < Order.Num(); ++I)
    {
        const FMcpCoplanarPiece& A = Pieces[Order[I]];
        const FBox Reach(A.Bounds.Min - Slack, A.Bounds.Max + Slack);
        for (int32 J = I + 1; J < Order.Num() && Pieces[Order[J]].Bounds.Min.X <= Reach.Max.X; ++J)
        {
            const FMcpCoplanarPiece& B = Pieces[Order[J]];
            if ((!A.bNamed && !B.bNamed) || !Reach.Intersect(B.Bounds))
            {
                continue;
            }
            // The smaller piece is the one to move: nudging a pillar beats shifting the ground under the level.
            const bool bASmaller = A.Volume <= B.Volume;
            FMcpCoplanarPair Best;
            for (int32 FaceA = 0; FaceA < A.Faces.Num(); ++FaceA)
            {
                for (int32 FaceB = 0; FaceB < B.Faces.Num(); ++FaceB)
                {
                    FMcpCoplanarPair Pair;
                    if (!FacesInOnePlane(A.Faces[FaceA], B.Faces[FaceB], Pair.Gap, Pair.OverlapU, Pair.OverlapV))
                    {
                        continue;
                    }
                    const FMcpCoplanarFace& Mine = bASmaller ? A.Faces[FaceA] : B.Faces[FaceB];
                    Pair.Piece = Order[bASmaller ? I : J];
                    Pair.Other = Order[bASmaller ? J : I];
                    Pair.Face = bASmaller ? FaceA : FaceB;
                    Pair.OtherFace = bASmaller ? FaceB : FaceA;
                    Pair.Normal = A.Faces[FaceA].Normal;
                    Pair.FaceArea = 4.0 * Mine.HalfU * Mine.HalfV;
                    if (!bBestOnly)
                    {
                        Pairs.Add(Pair);
                    }
                    else if (Pair.OverlapU * Pair.OverlapV > Best.OverlapU * Best.OverlapV)
                    {
                        Best = Pair;
                    }
                }
            }
            if (Best.OverlapU > 0.0)
            {
                Pairs.Add(Best);
            }
        }
    }
    Pairs.StableSort([](const FMcpCoplanarPair& A, const FMcpCoplanarPair& B) { return A.OverlapU * A.OverlapV > B.OverlapU * B.OverlapV; });
    return Pairs;
}

TArray<FMcpCoplanarPair> FindVisiblePairs(UWorld* World, const TArray<FMcpCoplanarPiece>& Pieces, bool bBestOnly)
{
    TArray<FMcpCoplanarPair> Pairs = FindPairs(Pieces, bBestOnly);
    Pairs.RemoveAll([World, &Pieces](const FMcpCoplanarPair& Pair)
    {
        const FMcpCoplanarFace& Mine = Pieces[Pair.Piece].Faces[Pair.Face];
        const FMcpCoplanarFace& Theirs = Pieces[Pair.Other].Faces[Pair.OtherFace];
        const FVector Smaller = Mine.HalfU * Mine.HalfV <= Theirs.HalfU * Theirs.HalfV ? Mine.Center : Theirs.Center;
        return McpCoplanarHiddenUnderside(World, Pair.Normal, Smaller, Pieces[Pair.Piece].Actor, Pieces[Pair.Other].Actor);
    });
    return Pairs;
}

TArray<FMcpCoplanarHit> FindCoplanarFaces(UWorld* World, const FString& NameFilter, const TArray<AActor*>* Focus)
{
    TArray<FMcpCoplanarHit> Hits;
    if (!World)
    {
        return Hits;
    }
    const TArray<FMcpCoplanarPiece> Pieces = CollectPieces(World, NameFilter, Focus);
    for (const FMcpCoplanarPair& Pair : FindVisiblePairs(World, Pieces, true))
    {
        FMcpCoplanarHit& Hit = Hits.AddDefaulted_GetRef();
        Hit.Actor = Pieces[Pair.Piece].Actor;
        Hit.OtherActor = Pieces[Pair.Other].Actor;
        Hit.Component = Pieces[Pair.Piece].Component->GetName();
        Hit.OtherComponent = Pieces[Pair.Other].Component->GetName();
        Hit.Normal = Pair.Normal;
        Hit.Gap = Pair.Gap;
        Hit.OverlapU = Pair.OverlapU;
        Hit.OverlapV = Pair.OverlapV;
    }
    return Hits;
}
} // namespace McpCoplanar
