#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"
#include "Domains/Skeleton/SkinWeights/McpAutomationBridge_SkeletonHandlersSkinWeightSource.h"

#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ReferenceSkeleton.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

// copy_weights and mirror_weights: rewrite a mesh's source skin weights from the nearest vertex of another
// mesh, or of the mirrored side of the same mesh, then rebuild so the weights survive every later build.
namespace McpSkeletonHandlers
{
namespace
{
using McpSkinSource::FInfluence;

// Adds Weight to Bone in Influences, merging with an existing entry for the same bone.
void AddTransferredInfluence(TArray<FInfluence>& Influences, FName Bone, float Weight)
{
    for (FInfluence& Existing : Influences)
    {
        if (Existing.Bone == Bone)
        {
            Existing.Weight += Weight;
            return;
        }
    }
    Influences.Add({Bone, Weight});
}

// The opposite-side bone of Bone when the mesh has one (thigh_l -> thigh_r, LeftHand -> RightHand); Bone otherwise.
FName MirroredBoneName(FName Bone, const TArray<FName>& Bones)
{
    static const TCHAR* const Pairs[][2] = {
        {TEXT("_l"), TEXT("_r")}, {TEXT("_L"), TEXT("_R")}, {TEXT(".L"), TEXT(".R")},
        {TEXT("Left"), TEXT("Right")}, {TEXT("left"), TEXT("right")}, {TEXT("l_"), TEXT("r_")}};
    const FString Name = Bone.ToString();
    for (const auto& Pair : Pairs)
    {
        for (int32 Way = 0; Way < 2; ++Way)
        {
            const FString From = Pair[Way];
            const FString To = Pair[1 - Way];
            FString Candidate = Name;
            if (From.EndsWith(TEXT("_")))
            {
                Candidate = Name.StartsWith(From, ESearchCase::CaseSensitive) ? To + Name.Mid(From.Len()) : Name;
            }
            else if (From.StartsWith(TEXT("_")) || From.StartsWith(TEXT(".")))
            {
                Candidate = Name.EndsWith(From, ESearchCase::CaseSensitive) ? Name.LeftChop(From.Len()) + To
                                                                             : Name.Replace(*(From + TEXT("_")), *(To + TEXT("_")), ESearchCase::CaseSensitive);
            }
            else
            {
                Candidate = Name.Replace(*From, *To, ESearchCase::CaseSensitive);
            }
            if (Candidate != Name && Bones.Contains(FName(*Candidate)))
            {
                return FName(*Candidate);
            }
        }
    }
    return Bone;
}

// Writes the new weights, reads them back for the before/after counts, saves, and replies.
bool CommitTransferredWeights(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const TSharedPtr<FJsonObject>& Payload, USkeletalMesh* Mesh, int32 LOD, const McpSkinSource::FSourceMesh& Before,
                              const McpSkinSource::FSourceMesh& After, TSharedPtr<FJsonObject> Result, const FString& Message)
{
    FString Error;
    FString Code;
    Result->SetArrayField(TEXT("morphTargetsDropped"), McpSkinSource::RenderOnlyMorphs(Mesh, Before));
    McpSkinSource::FSourceMesh Written;
    if (!McpSkinSource::Write(Mesh, LOD, After, true, {}, Error) || !McpSkinSource::Read(Mesh, LOD, false, Written, Code, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("WRITE_FAILED"));
        return true;
    }
    TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
    Counts->SetObjectField(TEXT("before"), McpSkinSource::BoneVertexCounts(Before));
    Counts->SetObjectField(TEXT("after"), McpSkinSource::BoneVertexCounts(Written));
    Result->SetObjectField(TEXT("boneVertexCounts"), Counts);
    Result->SetStringField(TEXT("skeletalMeshPath"), Mesh->GetPathName());
    Result->SetBoolField(TEXT("saved"), GetJsonBoolField(Payload, TEXT("save"), true) && SaveIfRequested(Mesh, Payload));
    McpHandlerUtils::AddVerification(Result, Mesh);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
}

bool HandleCopyWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString SourcePath = GetJsonStringField(Payload, TEXT("sourceMeshPath"));
    const FString TargetPath = GetJsonStringField(Payload, TEXT("targetMeshPath"));
    const int32 LOD = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    FString Error;
    FString Code;
    USkeletalMesh* SourceMesh = LoadSkeletalMeshFromPathSkel(SourcePath, Error);
    USkeletalMesh* TargetMesh = SourceMesh ? LoadSkeletalMeshFromPathSkel(TargetPath, Error) : nullptr;
    if (!SourceMesh || !TargetMesh || SourceMesh == TargetMesh)
    {
        Subsystem->SendAutomationError(Socket, RequestId, SourceMesh == TargetMesh && SourceMesh
            ? FString(TEXT("sourceMeshPath and targetMeshPath name the same mesh; to fix one side from the other use mirror_weights.")) : Error,
            SourceMesh == TargetMesh && SourceMesh ? TEXT("SAME_MESH") : TEXT("MESH_NOT_FOUND"));
        return true;
    }
    McpSkinSource::FSourceMesh Source;
    McpSkinSource::FSourceMesh Target;
    if (!McpSkinSource::Read(SourceMesh, 0, false, Source, Code, Error) || !McpSkinSource::Read(TargetMesh, LOD, true, Target, Code, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    const float Diagonal = McpSkinSource::BoundsDiagonal(Source.Positions);
    const McpSkinSource::FNearest Nearest(Source.Positions, Diagonal * 0.01f);
    McpSkinSource::FSourceMesh Result = Target;
    TSet<FName> Unmapped;
    int32 FarFromSource = 0;
    for (int32 Vertex = 0; Vertex < Target.Positions.Num(); ++Vertex)
    {
        float Distance = 0.0f;
        const int32 Match = Nearest.Find(Target.Positions[Vertex], Distance);
        FarFromSource += Distance > Diagonal * 0.02f ? 1 : 0;
        Result.Weights[Vertex].Reset();
        for (const FInfluence& Influence : Source.Weights[Match])
        {
            const FName Mapped = McpSkinSource::MapBone(SourceMesh->GetRefSkeleton(), Target.Bones, Influence.Bone);
            if (Mapped != Influence.Bone) Unmapped.Add(Influence.Bone);
            AddTransferredInfluence(Result.Weights[Vertex], Mapped, Influence.Weight);
        }
    }
    TSharedPtr<FJsonObject> Reply = McpHandlerUtils::CreateResultObject();
    Reply->SetNumberField(TEXT("verticesWritten"), Target.Positions.Num());
    Reply->SetNumberField(TEXT("verticesFarFromSource"), FarFromSource);
    TArray<TSharedPtr<FJsonValue>> UnmappedJson;
    for (const FName& Bone : Unmapped) UnmappedJson.Add(MakeShared<FJsonValueString>(Bone.ToString()));
    Reply->SetArrayField(TEXT("unmappedBones"), UnmappedJson);
    return CommitTransferredWeights(Subsystem, RequestId, Socket, Payload, TargetMesh, LOD, Target, Result, Reply,
        FString::Printf(TEXT("Copied skin weights from %s onto %d vertices of %s"), *SourceMesh->GetName(), Target.Positions.Num(), *TargetMesh->GetName()));
}

bool HandleMirrorWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString AxisText = GetJsonStringField(Payload, TEXT("axis"), TEXT("X"));
    const FString Direction = GetJsonStringField(Payload, TEXT("direction"), TEXT("positive_to_negative"));
    const int32 Axis = AxisText.Equals(TEXT("X"), ESearchCase::IgnoreCase) ? 0 : AxisText.Equals(TEXT("Y"), ESearchCase::IgnoreCase) ? 1 : AxisText.Equals(TEXT("Z"), ESearchCase::IgnoreCase) ? 2 : INDEX_NONE;
    const bool bPositiveSource = Direction.Equals(TEXT("positive_to_negative"), ESearchCase::IgnoreCase);
    if (Axis == INDEX_NONE || (!bPositiveSource && !Direction.Equals(TEXT("negative_to_positive"), ESearchCase::IgnoreCase)))
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("axis must be X, Y or Z and direction positive_to_negative or negative_to_positive."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Error;
    FString Code;
    const int32 LOD = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(GetJsonStringField(Payload, TEXT("skeletalMeshPath")), Error);
    McpSkinSource::FSourceMesh Source;
    if (!Mesh || !McpSkinSource::Read(Mesh, LOD, true, Source, Code, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, Mesh ? Code : FString(TEXT("MESH_NOT_FOUND")));
        return true;
    }
    TMap<FName, FName> Swap;
    TArray<TSharedPtr<FJsonValue>> PairsJson;
    for (const FName& Bone : Source.Bones)
    {
        const FName Other = MirroredBoneName(Bone, Source.Bones);
        Swap.Add(Bone, Other);
        if (Other != Bone)
        {
            TSharedPtr<FJsonObject> PairJson = MakeShared<FJsonObject>();
            PairJson->SetStringField(TEXT("from"), Bone.ToString());
            PairJson->SetStringField(TEXT("to"), Other.ToString());
            PairsJson.Add(MakeShared<FJsonValueObject>(PairJson));
        }
    }
    if (PairsJson.Num() == 0)
    {
        TArray<FString> Names;
        for (int32 Index = 0; Index < Source.Bones.Num() && Index < 40; ++Index) Names.Add(Source.Bones[Index].ToString());
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("No left and right bone pairs (_l and _r, Left and Right, .L and .R) in this mesh, so there is nothing to mirror into. Bones: %s"),
            *FString::Join(Names, TEXT(", "))), TEXT("NO_SIDE_BONES"));
        return true;
    }
    const float Tolerance = McpSkinSource::BoundsDiagonal(Source.Positions) * 0.005f;
    const McpSkinSource::FNearest Nearest(Source.Positions, Tolerance * 2.0f);
    McpSkinSource::FSourceMesh Result = Source;
    int32 Mirrored = 0;
    int32 Unmatched = 0;
    const float SourceSign = bPositiveSource ? 1.0f : -1.0f;
    for (int32 Vertex = 0; Vertex < Source.Positions.Num(); ++Vertex)
    {
        const FVector3f Position = Source.Positions[Vertex];
        if (Position[Axis] * SourceSign >= -Tolerance) continue; // source side or on the plane: kept
        FVector3f Reflected = Position;
        Reflected[Axis] = -Reflected[Axis];
        float Distance = 0.0f;
        const int32 Match = Nearest.Find(Reflected, Distance);
        if (Match == INDEX_NONE || Distance > Tolerance || Source.Positions[Match][Axis] * SourceSign < 0.0f)
        {
            ++Unmatched;
            continue;
        }
        Result.Weights[Vertex].Reset();
        for (const FInfluence& Influence : Source.Weights[Match])
        {
            AddTransferredInfluence(Result.Weights[Vertex], Swap.FindRef(Influence.Bone), Influence.Weight);
        }
        ++Mirrored;
    }
    TSharedPtr<FJsonObject> Reply = McpHandlerUtils::CreateResultObject();
    Reply->SetNumberField(TEXT("verticesMirrored"), Mirrored);
    Reply->SetNumberField(TEXT("unmatchedVertices"), Unmatched);
    Reply->SetArrayField(TEXT("bonePairs"), PairsJson);
    if (Mirrored == 0)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("No vertex on the destination side has a mirror partner within %.2f cm (%d unmatched); the mesh is not symmetric about %s."), Tolerance, Unmatched, *AxisText),
            TEXT("NOT_SYMMETRIC"));
        return true;
    }
    return CommitTransferredWeights(Subsystem, RequestId, Socket, Payload, Mesh, LOD, Source, Result, Reply,
        FString::Printf(TEXT("Mirrored skin weights onto %d vertices across %s"), Mirrored, *AxisText));
}
}
