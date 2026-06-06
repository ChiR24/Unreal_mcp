#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleGetSkeletonInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);

    // Try loading as skeletal mesh if skeleton load failed
    if (!Skeleton && !SkeletonPath.IsEmpty())
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Skeleton);

    // Bone count
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    Result->SetNumberField(TEXT("boneCount"), RefSkeleton.GetRawBoneNum());

    // Virtual bone count
    Result->SetNumberField(TEXT("virtualBoneCount"), Skeleton->GetVirtualBones().Num());

    // Socket count
    Result->SetNumberField(TEXT("socketCount"), Skeleton->Sockets.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeleton info retrieved"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleListBones(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);

    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

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

        // Use McpHandlerUtils helper for transform JSON
        const FTransform& RefPose = RefSkeleton.GetRefBonePose()[i];
        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        TransformObj->SetNumberField(TEXT("x"), RefPose.GetLocation().X);
        TransformObj->SetNumberField(TEXT("y"), RefPose.GetLocation().Y);
        TransformObj->SetNumberField(TEXT("z"), RefPose.GetLocation().Z);
        BoneObj->SetObjectField(TEXT("location"), TransformObj);

        BoneArray.Add(MakeShared<FJsonValueObject>(BoneObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Skeleton);
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
#if WITH_EDITOR
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    int32 LODIndex = GetIntFieldSkel(Payload, TEXT("lodIndex"), 0);

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    const FReferenceSkeleton* RefSkeleton = nullptr;
    FString SourcePath;

    if (!SkeletalMeshPath.IsEmpty())
    {
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return true;
        }
        RefSkeleton = &Mesh->GetRefSkeleton();
        SourcePath = SkeletalMeshPath;
    }
    else if (!SkeletonPath.IsEmpty())
    {
        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return true;
        }
        RefSkeleton = &Skeleton->GetReferenceSkeleton();
        SourcePath = SkeletonPath;
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletalMeshPath or skeletonPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    int32 BoneIndex = RefSkeleton->FindBoneIndex(FName(*BoneName));
    if (BoneIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }

    FTransform BoneTransform = RefSkeleton->GetRefBonePose()[BoneIndex];
    FVector Location = BoneTransform.GetLocation();
    FRotator Rotation = BoneTransform.Rotator();
    FVector Scale = BoneTransform.GetScale3D();

    // Get parent info
    int32 ParentIndex = RefSkeleton->GetParentIndex(BoneIndex);
    FString ParentName = ParentIndex != INDEX_NONE ?
        RefSkeleton->GetBoneName(ParentIndex).ToString() : TEXT("");

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);
    Result->SetStringField(TEXT("parentBone"), ParentName);
    Result->SetNumberField(TEXT("parentIndex"), ParentIndex);

    TSharedPtr<FJsonObject> LocationObj = McpHandlerUtils::CreateResultObject();
    LocationObj->SetNumberField(TEXT("x"), Location.X);
    LocationObj->SetNumberField(TEXT("y"), Location.Y);
    LocationObj->SetNumberField(TEXT("z"), Location.Z);
    Result->SetObjectField(TEXT("location"), LocationObj);

    TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
    RotationObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
    RotationObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
    RotationObj->SetNumberField(TEXT("roll"), Rotation.Roll);
    Result->SetObjectField(TEXT("rotation"), RotationObj);

    TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
    ScaleObj->SetNumberField(TEXT("x"), Scale.X);
    ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
    ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
    Result->SetObjectField(TEXT("scale"), ScaleObj);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Retrieved transform for bone '%s'"), *BoneName), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("get_bone_transform requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
