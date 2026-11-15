#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "AssetCompilingManager.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/McpScopedEditorTransaction.h"

#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
#include "BoneWeights.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#else
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#endif

namespace McpSkinSource
{
namespace
{
constexpr float MorphDeltaEpsilon = 1e-4f;
}

bool Write(USkeletalMesh* Mesh, int32 LOD, const FSourceMesh& In, bool bWeights, const TArray<FName>& MorphNames, FString& OutError, FName Profile)
{
    FString Code;
    if (!CheckSourceLod(Mesh, LOD, Code, OutError) || !CheckProfile(Profile, Code, OutError))
    {
        return false;
    }
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
    // One undo step for the edit, closed when the write returns. The mesh description is bulk data outside the mesh's
    // properties: ModifyMeshDescription stores it in the open step (the gate has made the mesh's own record).
    FMcpScopedEditorTransaction Transaction(FText::FromString(MorphNames.Num() > 0 ? TEXT("Edit Morph Target") : TEXT("Edit Skin Weights")),
        EMcpMutationDurability::EditorStateOnly, {Mesh});
    Mesh->ModifyMeshDescription(LOD);
    FMeshDescription* Description = Mesh->GetMeshDescription(LOD);
    if (!Description)
    {
        Transaction.Cancel();
        OutError = TEXT("The mesh description could not be loaded.");
        return false;
    }
    FSkeletalMeshAttributes Attributes(*Description);
    if (bWeights)
    {
        if (!Profile.IsNone() && !Attributes.GetSkinWeightProfileNames().Contains(Profile) && !Attributes.RegisterSkinWeightAttribute(Profile))
        {
            Transaction.Cancel();
            OutError = FString::Printf(TEXT("'%s' cannot name a skin weight profile."), *Profile.ToString());
            return false;
        }
        FSkinWeightsVertexAttributesRef SkinWeights = Attributes.GetVertexSkinWeights(Profile);
        for (int32 Index = 0; Index < In.VertexIds.Num(); ++Index)
        {
            TArray<UE::AnimationCore::FBoneWeight> BoneWeights;
            for (const FInfluence& Influence : In.Weights[Index])
            {
                const int32 Bone = In.Bones.IndexOfByKey(Influence.Bone);
                if (Bone != INDEX_NONE && Influence.Weight > 0.0f)
                {
                    BoneWeights.Add(UE::AnimationCore::FBoneWeight(static_cast<uint16>(Bone), Influence.Weight));
                }
            }
            // The default settings always normalize (FBoneWeightsSettings::NormalizeType is Always).
            SkinWeights.Set(FVertexID(In.VertexIds[Index]), TArrayView<const UE::AnimationCore::FBoneWeight>(BoneWeights.GetData(), BoneWeights.Num()));
        }
    }
    for (const FName& MorphName : MorphNames)
    {
        const TArray<FVector3f>* Deltas = In.Morphs.Find(MorphName);
        if (!Deltas || Deltas->Num() < In.VertexIds.Num())
        {
            OutError = FString::Printf(TEXT("Morph target '%s' could not be written."), *MorphName.ToString());
            return false;
        }
        Attributes.RegisterMorphTargetAttribute(MorphName, false);
        TVertexAttributesRef<FVector3f> Target = Attributes.GetVertexMorphPositionDelta(MorphName);
        if (!Target.IsValid())
        {
            OutError = FString::Printf(TEXT("Morph target '%s' could not be written."), *MorphName.ToString());
            return false;
        }
        for (int32 Index = 0; Index < In.VertexIds.Num(); ++Index)
        {
            const FVector3f Delta = (*Deltas)[Index];
            Target.Set(FVertexID(In.VertexIds[Index]), Delta.SizeSquared() > MorphDeltaEpsilon * MorphDeltaEpsilon ? Delta : FVector3f::ZeroVector);
        }
    }
    Mesh->CommitMeshDescription(LOD);
#else
    Mesh->Modify();
    FSkeletalMeshImportData Data;
    Mesh->LoadLODImportedData(LOD, Data);
    if (bWeights)
    {
        Data.Influences.Reset();
        for (int32 Vertex = 0; Vertex < In.Weights.Num() && Vertex < Data.Points.Num(); ++Vertex)
        {
            for (const FInfluence& Influence : In.Weights[Vertex])
            {
                const int32 Bone = In.Bones.IndexOfByKey(Influence.Bone);
                if (Bone == INDEX_NONE || Influence.Weight <= 0.0f) continue;
                SkeletalMeshImportData::FRawBoneInfluence& Raw = Data.Influences.AddDefaulted_GetRef();
                Raw.Weight = Influence.Weight;
                Raw.VertexIndex = Vertex;
                Raw.BoneIndex = Bone;
            }
        }
        // Sorts, normalizes and caps the influences the way an import does.
        SkeletalMeshImportUtils::ProcessImportMeshInfluences(Data, Mesh->GetPathName());
    }
    for (const FName& MorphName : MorphNames)
    {
        const TArray<FVector3f>* Deltas = In.Morphs.Find(MorphName);
        if (!Deltas)
        {
            OutError = FString::Printf(TEXT("Morph target '%s' could not be written."), *MorphName.ToString());
            return false;
        }
        int32 Slot = Data.MorphTargetNames.IndexOfByKey(MorphName.ToString());
        if (Slot == INDEX_NONE)
        {
            Slot = Data.MorphTargetNames.Add(MorphName.ToString());
            Data.MorphTargets.AddDefaulted();
            Data.MorphTargetModifiedPoints.AddDefaulted();
        }
        FSkeletalMeshImportData Shape;
        TSet<uint32> Modified;
        for (int32 Point = 0; Point < Data.Points.Num() && Point < Deltas->Num(); ++Point)
        {
            if ((*Deltas)[Point].SizeSquared() > MorphDeltaEpsilon * MorphDeltaEpsilon)
            {
                Modified.Add(static_cast<uint32>(Point));
                Shape.Points.Add(Data.Points[Point] + (*Deltas)[Point]);
            }
        }
        Data.MorphTargets[Slot] = MoveTemp(Shape);
        Data.MorphTargetModifiedPoints[Slot] = MoveTemp(Modified);
    }
    Mesh->SaveLODImportedData(LOD, Data);
#endif
    // A null-property PostEditChange rebuilds the render data from the source data; the build may run async.
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    return true;
}
}
