#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "Engine/SkeletalMesh.h"

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

// Before UE 5.4 the source data keeps an alternate skin weight profile as a whole imported mesh, which nothing here
// writes; from 5.4 a profile is a vertex attribute of the mesh description.
bool CheckProfile(FName Profile, FString& OutCode, FString& OutError)
{
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
    return true;
#else
    if (Profile.IsNone())
    {
        return true;
    }
    OutCode = TEXT("PROFILE_UNSUPPORTED");
    OutError = TEXT("This engine version keeps a skin weight profile in the mesh's source data as a whole imported mesh, which this edit does not write (UE 5.4 and later keep it as vertex weights).");
    return false;
#endif
}

bool Read(USkeletalMesh* Mesh, int32 LOD, bool bWithMorphs, FSourceMesh& Out, FString& OutCode, FString& OutError, FName Profile)
{
    Out = FSourceMesh();
    if (!CheckSourceLod(Mesh, LOD, OutCode, OutError) || !CheckProfile(Profile, OutCode, OutError))
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
    // A profile the source data lacks starts as a copy of the default weights.
    const bool bHasProfile = !Profile.IsNone() && Attributes.GetSkinWeightProfileNames().Contains(Profile);
    const FSkinWeightsVertexAttributesConstRef SkinWeights = Attributes.GetVertexSkinWeights(bHasProfile ? Profile : NAME_None);
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
}
