#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleGetSkeletonInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonOrMeshSkeleton(SkeletonPath, Error);

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Skeleton);

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    Result->SetNumberField(TEXT("boneCount"), RefSkeleton.GetRawBoneNum());

    Result->SetNumberField(TEXT("virtualBoneCount"), Skeleton->GetVirtualBones().Num());

    Result->SetNumberField(TEXT("socketCount"), Skeleton->Sockets.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeleton info retrieved"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleListBones(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonOrMeshSkeleton(SkeletonPath, Error);

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    TArray<TSharedPtr<FJsonValue>> BoneArray;

    for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i)
    {
        TSharedPtr<FJsonObject> BoneObj = McpHandlerUtils::CreateResultObject();
        BoneObj->SetStringField(TEXT("name"), RefSkeleton.GetBoneName(i).ToString());
        BoneObj->SetNumberField(TEXT("index"), i);

        int32 ParentIndex = RefSkeleton.GetParentIndex(i);
        BoneObj->SetNumberField(TEXT("parentIndex"), ParentIndex);
        if (ParentIndex >= 0)
        {
            BoneObj->SetStringField(TEXT("parentName"), RefSkeleton.GetBoneName(ParentIndex).ToString());
        }

        const FTransform& RefPose = RefSkeleton.GetRefBonePose()[i];
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetNumberField(TEXT("x"), RefPose.GetLocation().X);
        TransformObj->SetNumberField(TEXT("y"), RefPose.GetLocation().Y);
        TransformObj->SetNumberField(TEXT("z"), RefPose.GetLocation().Z);
        BoneObj->SetObjectField(TEXT("location"), TransformObj);

        BoneArray.Add(MakeShared<FJsonValueObject>(BoneObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("bones"), BoneArray);
    Result->SetNumberField(TEXT("count"), BoneArray.Num());
    McpHandlerUtils::AddVerification(Result, Skeleton);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bones listed"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleGetBoneTransform(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Same resolution as set_bone_transform, so a get/set pair reads and
    // writes one reference skeleton: the mesh's when a mesh is addressed or
    // reachable from the skeleton, else the skeleton's own reference pose.
    const FSkeletonMeshTarget Target = ResolveSkeletonMeshTarget(Payload);
    if (!Target.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, Target.Error, Target.ErrorCode);
        return true;
    }
    const FReferenceSkeleton* RefSkeleton = Target.Mesh
        ? &Target.Mesh->GetRefSkeleton()
        : &Target.Skeleton->GetReferenceSkeleton();

    const int32 BoneIndex = RefSkeleton->FindBoneIndex(FName(*BoneName));
    if (BoneIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }

    const int32 ParentIndex = RefSkeleton->GetParentIndex(BoneIndex);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);
    Result->SetStringField(TEXT("parentBone"),
        ParentIndex != INDEX_NONE ? RefSkeleton->GetBoneName(ParentIndex).ToString() : FString());
    Result->SetNumberField(TEXT("parentIndex"), ParentIndex);
    Result->SetStringField(TEXT("source"), Target.Mesh ? TEXT("skeletalMesh") : TEXT("skeleton"));
    if (Target.Mesh)
    {
        Result->SetStringField(TEXT("skeletalMeshPath"), Target.Mesh->GetPathName());
    }
    if (Target.Skeleton)
    {
        Result->SetStringField(TEXT("skeletonPath"), Target.Skeleton->GetPathName());
    }
    WriteTransformToJson(RefSkeleton->GetRefBonePose()[BoneIndex], Result);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Retrieved transform for bone '%s'"), *BoneName), Result);
    return true;
}

#endif // WITH_EDITOR
