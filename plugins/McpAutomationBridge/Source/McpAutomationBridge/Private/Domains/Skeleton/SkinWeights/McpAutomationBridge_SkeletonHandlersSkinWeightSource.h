#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Spatial/PointHashGrid3.h"

class USkeletalMesh;
struct FReferenceSkeleton;

// A skeletal mesh's SOURCE data for one LOD: FSkeletalMeshImportData before UE 5.4, the mesh description
// from 5.4. Build() regenerates render data from it, so an edit made only to render data is lost on the
// next build; copy, mirror and prune weights and imported morph targets all go through here.
namespace McpSkinSource
{
struct FInfluence
{
    FName Bone;
    float Weight = 0.0f;
};

struct FSourceMesh
{
    TArray<int32> VertexIds;              // the source data's own id of each vertex
    TArray<FVector3f> Positions;          // per vertex
    TArray<TArray<FInfluence>> Weights;   // per vertex
    TArray<FName> Bones;                  // bones the source data can hold, in its own index order
    TMap<FName, TArray<FVector3f>> Morphs; // per-vertex position deltas, zero where a morph leaves a vertex alone
};

// Reads LOD of Mesh. False, with OutCode and OutError, when the mesh has no source data there.
bool Read(USkeletalMesh* Mesh, int32 LOD, bool bWithMorphs, FSourceMesh& Out, FString& OutCode, FString& OutError);
// Writes In.Weights (when bWeights) and the Morphs named in MorphNames into the source data, rebuilds the mesh
// (which normalizes and caps the influences) and waits for the build. Positions are never written.
bool Write(USkeletalMesh* Mesh, int32 LOD, const FSourceMesh& In, bool bWeights, const TArray<FName>& MorphNames, FString& OutError);
// The bone of TargetBones that stands in for SourceBone: the same name, else the nearest ancestor in
// SourceRef that TargetBones has, else the first target bone (the root).
FName MapBone(const FReferenceSkeleton& SourceRef, const TArray<FName>& TargetBones, FName SourceBone);
// Bone name -> number of vertices it influences.
TSharedPtr<FJsonObject> BoneVertexCounts(const FSourceMesh& Mesh);
// Morph targets the mesh renders that its source data lacks (Source read with morphs): a rebuild drops them.
TArray<TSharedPtr<FJsonValue>> RenderOnlyMorphs(USkeletalMesh* Mesh, const FSourceMesh& Source);
// Length of the diagonal of the bounds of Positions.
float BoundsDiagonal(const TArray<FVector3f>& Positions);

// Nearest point of a fixed set, through a hash grid (cells of CellSize) with a brute-force fallback.
class FNearest
{
public:
    FNearest(const TArray<FVector3f>& InPoints, float InCellSize)
        : Points(InPoints), CellSize(FMath::Max(InCellSize, 1e-3f)), Grid(CellSize, INDEX_NONE)
    {
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            Grid.InsertPointUnsafe(Index, Points[Index]);
        }
    }

    // Index of the nearest point (INDEX_NONE when the set is empty); OutDistance is its distance.
    int32 Find(const FVector3f& Query, float& OutDistance) const
    {
        const auto DistSq = [this, &Query](const int32& Index) { return FVector3f::DistSquared(Points[Index], Query); };
        for (const float Radius : {CellSize, CellSize * 8.0f})
        {
            const TPair<int32, float> Hit = Grid.FindNearestInRadius(Query, Radius, DistSq);
            if (Hit.Key != INDEX_NONE)
            {
                OutDistance = FMath::Sqrt(Hit.Value);
                return Hit.Key;
            }
        }
        // ponytail: brute force for a point far from every other; a k-d tree if meshes this far apart get big.
        int32 Best = INDEX_NONE;
        float BestSq = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const float Sq = DistSq(Index);
            if (Sq < BestSq)
            {
                BestSq = Sq;
                Best = Index;
            }
        }
        OutDistance = Best == INDEX_NONE ? 0.0f : FMath::Sqrt(BestSq);
        return Best;
    }

private:
    const TArray<FVector3f>& Points;
    float CellSize;
    UE::Geometry::TPointHashGrid3f<int32> Grid;
};
}
