#include "Domains/Skeleton/SkinWeights/McpAutomationBridge_SkeletonHandlersSkinWeightSource.h"

#include "AssetCompilingManager.h"
#include "Engine/SkeletalMesh.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MINOR_VERSION >= 4
#define MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION 1
#include "BoneWeights.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#else
#define MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION 0
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#endif

namespace McpSkinSource
{
namespace
{
constexpr float MorphDeltaEpsilon = 1e-4f;

bool CheckSourceLod(USkeletalMesh* Mesh, int32 LOD, FString& OutCode, FString& OutError)
{
    if (!Mesh || LOD < 0 || LOD >= Mesh->GetLODNum())
    {
        OutCode = TEXT("INVALID_LOD");
        OutError = FString::Printf(TEXT("lodIndex %d does not exist (the mesh has %d LODs)."), LOD, Mesh ? Mesh->GetLODNum() : 0);
        return false;
    }
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
    const bool bHasSource = Mesh->HasMeshDescription(LOD);
#else
    const bool bHasSource = Mesh->IsLODImportedDataBuildAvailable(LOD);
#endif
    if (!bHasSource)
    {
        OutCode = TEXT("SOURCE_DATA_MISSING");
        OutError = FString::Printf(TEXT("'%s' LOD %d has no source geometry to edit (a generated LOD, or a mesh imported before source data was kept). Re-import the mesh once, or edit LOD 0."),
            *Mesh->GetName(), LOD);
        return false;
    }
    return true;
}
}

bool Read(USkeletalMesh* Mesh, int32 LOD, bool bWithMorphs, FSourceMesh& Out, FString& OutCode, FString& OutError)
{
    Out = FSourceMesh();
    if (!CheckSourceLod(Mesh, LOD, OutCode, OutError))
    {
        return false;
    }
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
    const FMeshDescription* Description = Mesh->GetMeshDescription(LOD);
    if (!Description)
    {
        OutCode = TEXT("SOURCE_DATA_MISSING");
        OutError = TEXT("The mesh description could not be loaded.");
        return false;
    }
    FSkeletalMeshConstAttributes Attributes(*Description);
    const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
    const auto BoneNames = Attributes.GetBoneNames();
    if (Attributes.GetNumBones() > 0 && BoneNames.IsValid())
    {
        for (int32 Bone = 0; Bone < Attributes.GetNumBones(); ++Bone)
        {
            Out.Bones.Add(BoneNames.Get(FBoneID(Bone)));
        }
    }
    else
    {
        for (int32 Bone = 0; Bone < Mesh->GetRefSkeleton().GetRawBoneNum(); ++Bone)
        {
            Out.Bones.Add(Mesh->GetRefSkeleton().GetBoneName(Bone));
        }
    }
    const FSkinWeightsVertexAttributesConstRef SkinWeights = Attributes.GetVertexSkinWeights();
    for (const FVertexID VertexID : Description->Vertices().GetElementIDs())
    {
        Out.VertexIds.Add(VertexID.GetValue());
        Out.Positions.Add(Positions.Get(VertexID));
        TArray<FInfluence>& Influences = Out.Weights.AddDefaulted_GetRef();
        for (const UE::AnimationCore::FBoneWeight BoneWeight : SkinWeights.Get(VertexID))
        {
            if (Out.Bones.IsValidIndex(BoneWeight.GetBoneIndex()))
            {
                Influences.Add({Out.Bones[BoneWeight.GetBoneIndex()], BoneWeight.GetWeight()});
            }
        }
    }
    if (bWithMorphs)
    {
        for (const FName MorphName : Attributes.GetMorphTargetNames())
        {
            const TVertexAttributesConstRef<FVector3f> Deltas = Attributes.GetVertexMorphPositionDelta(MorphName);
            TArray<FVector3f>& Target = Out.Morphs.Add(MorphName);
            for (const int32 Id : Out.VertexIds)
            {
                Target.Add(Deltas.IsValid() ? Deltas.Get(FVertexID(Id)) : FVector3f::ZeroVector);
            }
        }
    }
#else
    FSkeletalMeshImportData Data;
    Mesh->LoadLODImportedData(LOD, Data);
    for (const SkeletalMeshImportData::FBone& Bone : Data.RefBonesBinary)
    {
        Out.Bones.Add(FName(*Bone.Name));
    }
    Out.Positions = Data.Points;
    Out.Weights.SetNum(Data.Points.Num());
    for (int32 Index = 0; Index < Data.Points.Num(); ++Index)
    {
        Out.VertexIds.Add(Index);
    }
    for (const SkeletalMeshImportData::FRawBoneInfluence& Influence : Data.Influences)
    {
        if (Out.Weights.IsValidIndex(Influence.VertexIndex) && Out.Bones.IsValidIndex(Influence.BoneIndex))
        {
            Out.Weights[Influence.VertexIndex].Add({Out.Bones[Influence.BoneIndex], Influence.Weight});
        }
    }
    // A morph stores the moved points only, in the iteration order of its modified-point set.
    for (int32 Morph = 0; bWithMorphs && Morph < Data.MorphTargetNames.Num(); ++Morph)
    {
        if (!Data.MorphTargets.IsValidIndex(Morph) || !Data.MorphTargetModifiedPoints.IsValidIndex(Morph)) continue;
        TArray<FVector3f>& Target = Out.Morphs.Add(FName(*Data.MorphTargetNames[Morph]));
        Target.SetNumZeroed(Data.Points.Num());
        int32 Packed = 0;
        for (const uint32 Point : Data.MorphTargetModifiedPoints[Morph])
        {
            if (Target.IsValidIndex(static_cast<int32>(Point)) && Data.MorphTargets[Morph].Points.IsValidIndex(Packed))
            {
                Target[Point] = Data.MorphTargets[Morph].Points[Packed] - Data.Points[Point];
            }
            ++Packed;
        }
    }
#endif
    if (Out.Positions.Num() == 0)
    {
        OutCode = TEXT("SOURCE_DATA_MISSING");
        OutError = FString::Printf(TEXT("'%s' LOD %d has no source vertices."), *Mesh->GetName(), LOD);
        return false;
    }
    return true;
}

bool Write(USkeletalMesh* Mesh, int32 LOD, const FSourceMesh& In, bool bWeights, const TArray<FName>& MorphNames, FString& OutError)
{
    FString Code;
    if (!CheckSourceLod(Mesh, LOD, Code, OutError))
    {
        return false;
    }
    Mesh->Modify();
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
    FMeshDescription* Description = Mesh->GetMeshDescription(LOD);
    if (!Description)
    {
        OutError = TEXT("The mesh description could not be loaded.");
        return false;
    }
    FSkeletalMeshAttributes Attributes(*Description);
    if (bWeights)
    {
        FSkinWeightsVertexAttributesRef SkinWeights = Attributes.GetVertexSkinWeights();
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
        Attributes.RegisterMorphTargetAttribute(MorphName, false);
        TVertexAttributesRef<FVector3f> Target = Attributes.GetVertexMorphPositionDelta(MorphName);
        if (!Deltas || !Target.IsValid())
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
