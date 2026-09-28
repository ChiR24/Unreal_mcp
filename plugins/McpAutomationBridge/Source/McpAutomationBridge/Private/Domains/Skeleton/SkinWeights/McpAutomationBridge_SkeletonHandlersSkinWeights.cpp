#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"
#include "Domains/Skeleton/SkinWeights/McpAutomationBridge_SkeletonHandlersSkinWeightSource.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "ReferenceSkeleton.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

// Source-data helpers shared by the skin-weight and morph-target edits, and prune_weights.
namespace McpSkinSource
{
FName MapBone(const FReferenceSkeleton& SourceRef, const TArray<FName>& TargetBones, FName SourceBone)
{
    for (int32 Bone = SourceRef.FindBoneIndex(SourceBone); Bone != INDEX_NONE; Bone = SourceRef.GetParentIndex(Bone))
    {
        if (TargetBones.Contains(SourceRef.GetBoneName(Bone)))
        {
            return SourceRef.GetBoneName(Bone);
        }
    }
    return TargetBones.Contains(SourceBone) ? SourceBone : (TargetBones.Num() > 0 ? TargetBones[0] : NAME_None);
}

TSharedPtr<FJsonObject> BoneVertexCounts(const FSourceMesh& Mesh)
{
    TMap<FName, int32> Counts;
    for (const TArray<FInfluence>& Influences : Mesh.Weights)
    {
        for (const FInfluence& Influence : Influences)
        {
            Counts.FindOrAdd(Influence.Bone) += Influence.Weight > 0.0f ? 1 : 0;
        }
    }
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    for (const TPair<FName, int32>& Pair : Counts)
    {
        Json->SetNumberField(Pair.Key.ToString(), Pair.Value);
    }
    return Json;
}

bool CheckRenderOnlyMorphs(USkeletalMesh* Mesh, const FSourceMesh& Source, bool bAllowDrop, TArray<TSharedPtr<FJsonValue>>& OutDropped, FString& OutError)
{
    TArray<FString> Names;
    for (const UMorphTarget* Morph : Mesh->GetMorphTargets())
    {
        if (Morph && !Source.Morphs.Contains(Morph->GetFName()))
        {
            Names.Add(Morph->GetName());
            OutDropped.Add(MakeShared<FJsonValueString>(Morph->GetName()));
        }
    }
    if (bAllowDrop || Names.Num() == 0)
    {
        return true;
    }
    OutError = FString::Printf(TEXT("%s has morph targets that exist only in its render data (%s). This edit rebuilds the mesh from its source data, which deletes them, so nothing was changed. Pass dropRenderOnlyMorphs true to accept losing them."),
        *Mesh->GetName(), *FString::Join(Names, TEXT(", ")));
    return false;
}

float BoundsDiagonal(const TArray<FVector3f>& Positions)
{
    FBox3f Box(ForceInit);
    for (const FVector3f& Position : Positions)
    {
        Box += Position;
    }
    return Box.IsValid ? (Box.Max - Box.Min).Size() : 0.0f;
}
}

namespace McpSkeletonHandlers
{
// prune_weights: drop influences below threshold (always keeping each vertex's strongest), renormalize, rebuild.
bool HandlePruneWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const double Threshold = GetJsonNumberField(Payload, TEXT("threshold"), 0.01);
    const int32 LOD = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    if (!(Threshold > 0.0 && Threshold < 0.5))
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("threshold must be above 0 and below 0.5; a larger cut would stiffen blended joints."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Error;
    FString Code;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(GetJsonStringField(Payload, TEXT("skeletalMeshPath")), Error);
    McpSkinSource::FSourceMesh Source;
    if (!Mesh || !McpSkinSource::Read(Mesh, LOD, true, Source, Code, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, Mesh ? Code : FString(TEXT("MESH_NOT_FOUND")));
        return true;
    }
    int32 Removed = 0;
    int32 VerticesChanged = 0;
    int32 MaxBefore = 0;
    int32 MaxAfter = 0;
    for (TArray<McpSkinSource::FInfluence>& Influences : Source.Weights)
    {
        int32 Strongest = INDEX_NONE;
        for (int32 Index = 0; Index < Influences.Num(); ++Index)
        {
            if (Strongest == INDEX_NONE || Influences[Index].Weight > Influences[Strongest].Weight) Strongest = Index;
        }
        TArray<McpSkinSource::FInfluence> Kept;
        for (int32 Index = 0; Index < Influences.Num(); ++Index)
        {
            if (Index == Strongest || Influences[Index].Weight >= Threshold) Kept.Add(Influences[Index]);
        }
        MaxBefore = FMath::Max(MaxBefore, Influences.Num());
        MaxAfter = FMath::Max(MaxAfter, Kept.Num());
        Removed += Influences.Num() - Kept.Num();
        VerticesChanged += Influences.Num() != Kept.Num() ? 1 : 0;
        Influences = MoveTemp(Kept);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), Mesh->GetPathName());
    Result->SetNumberField(TEXT("influencesRemoved"), Removed);
    Result->SetNumberField(TEXT("verticesChanged"), VerticesChanged);
    Result->SetNumberField(TEXT("maxInfluencesBefore"), MaxBefore);
    Result->SetNumberField(TEXT("maxInfluencesAfter"), MaxAfter);
    bool bSaved = false;
    // Nothing under the threshold: no rebuild and no save, so a repeat call is free.
    if (Removed > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Dropped;
        if (!McpSkinSource::CheckRenderOnlyMorphs(Mesh, Source, GetJsonBoolField(Payload, TEXT("dropRenderOnlyMorphs"), false), Dropped, Error))
        {
            Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("RENDER_ONLY_MORPHS"));
            return true;
        }
        Result->SetArrayField(TEXT("morphTargetsDropped"), Dropped);
        if (!McpSkinSource::Write(Mesh, LOD, Source, true, {}, Error))
        {
            Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("WRITE_FAILED"));
            return true;
        }
        bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && SaveIfRequested(Mesh, Payload);
    }
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Mesh);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, FString::Printf(
        TEXT("Removed %d influences below %.3f from %d vertices"), Removed, Threshold, VerticesChanged), Result);
    return true;
}
}
