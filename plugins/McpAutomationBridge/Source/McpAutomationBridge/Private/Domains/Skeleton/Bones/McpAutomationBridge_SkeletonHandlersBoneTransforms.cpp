#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleSetBoneTransform(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("boneName is required (with skeletalMeshPath or skeletonPath)"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Resolve the mesh whose reference skeleton get_bone_transform reads, so a
    // get/set pair always addresses one set of data. skeletonPath resolves to
    // the skeleton's preview or compatible mesh (dogfood #72, #93).
    const FSkeletonMeshTarget Target = ResolveSkeletonMeshTarget(Payload);
    if (!Target.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, Target.Error, Target.ErrorCode);
        return true;
    }
    USkeletalMesh* Mesh = Target.Mesh;
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Skeleton '%s' has no preview or compatible skeletal mesh to write a reference pose to; pass skeletalMeshPath"), *Target.SourcePath),
            TEXT("SKELETON_HAS_NO_MESH"));
        return true;
    }

    FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    const int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
    if (BoneIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone '%s' not found on %s"), *BoneName, *Mesh->GetPathName()), TEXT("BONE_NOT_FOUND"));
        return true;
    }

    // Start from the current reference pose and overwrite only the components
    // the caller supplied; a fresh transform reset every unmentioned component
    // to identity (silent ref-pose corruption).
    FTransform NewTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
    const int32 AppliedComponents = ApplyTransformFieldsFromJson(Payload, NewTransform);
    if (AppliedComponents == 0)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Provide at least one of location [x,y,z], rotation [pitch,yaw,roll] or scale (number or [x,y,z])"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    Mesh->Modify();
    {
        // The modifier rebuilds the bone maps when it leaves scope.
        FReferenceSkeletonModifier Modifier(RefSkeleton, Mesh->GetSkeleton());
        Modifier.UpdateRefPoseTransform(BoneIndex, NewTransform);
    }
    Mesh->CalculateInvRefMatrices();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);
    Result->SetStringField(TEXT("skeletalMeshPath"), Mesh->GetPathName());
    if (Target.Skeleton)
    {
        Result->SetStringField(TEXT("skeletonPath"), Target.Skeleton->GetPathName());
    }
    Result->SetNumberField(TEXT("appliedComponents"), AppliedComponents);
    // Echo what the reference skeleton now holds, not what was requested.
    WriteTransformToJson(Mesh->GetRefSkeleton().GetRefBonePose()[BoneIndex], Result);
    McpHandlerUtils::AddVerification(Result, Mesh);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Bone '%s' reference transform updated on %s"), *BoneName, *Mesh->GetName()), Result);
    return true;
}

#endif // WITH_EDITOR
