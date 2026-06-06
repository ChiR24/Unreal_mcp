#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "ReferenceSkeleton.h"
#if __has_include("Animation/SkeletonModifier.h")
#include "Animation/SkeletonModifier.h"
#endif

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleSetBoneTransform(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    // Also accept skeletonPath for backward compatibility
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    }
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (SkeletalMeshPath.IsEmpty() || BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletalMeshPath (or skeletonPath) and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));

    if (BoneIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return true;
    }

    // Parse transform
    FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
    FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);

    FTransform NewTransform(Rotation, Location, Scale);

    // Modify the reference skeleton
    // Note: This modifies the skeleton in memory. For persistent changes, the mesh needs to be reimported.
    FReferenceSkeletonModifier Modifier(Mesh->GetRefSkeleton(), Mesh->GetSkeleton());
    Modifier.UpdateRefPoseTransform(BoneIndex, NewTransform);

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Bone '%s' transform updated"), *BoneName), Result);
    return true;
}

#endif // WITH_EDITOR
