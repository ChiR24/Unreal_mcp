#include "McpAutomationBridge_SkeletonHandlersActions.h"
#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "ReferenceSkeleton.h"
#if __has_include("Animation/SkeletonModifier.h")
#include "Animation/SkeletonModifier.h"
#endif

#if WITH_EDITOR

namespace McpSkeletonHandlers {

bool HandleRemoveBoneAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        bool bRemoveChildren = false;
        Payload->TryGetBoolField(TEXT("removeChildren"), bRemoveChildren);

        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return true;
        }

        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));

        if (BoneIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
            return true;
        }

        // Check if it's the root bone
        if (BoneIndex == 0)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot remove root bone"), TEXT("CANNOT_REMOVE_ROOT"));
            return true;
        }

        // Remove the bone using FReferenceSkeletonModifier
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FReferenceSkeletonModifier Modifier(Skeleton);
        Modifier.Remove(FName(*BoneName), bRemoveChildren);
        McpSafeAssetSave(Skeleton);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("removedBone"), BoneName);
        Result->SetBoolField(TEXT("childrenRemoved"), bRemoveChildren);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' removed from skeleton"), *BoneName), Result);
        return true;
#else
        // UE 5.0-5.2: FReferenceSkeletonModifier doesn't have Remove() method
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            TEXT("remove_bone is not supported in UE 5.0-5.2. Please use UE 5.3 or later."),
            TEXT("NOT_SUPPORTED"));
        return true;
#endif
}

bool HandleSetBoneParentAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        FString NewParentName = GetStringFieldSkel(Payload, TEXT("parentBone"));
        if (NewParentName.IsEmpty())
        {
            NewParentName = GetStringFieldSkel(Payload, TEXT("newParentBone"));
        }

        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return true;
        }

        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));

        if (BoneIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
            return true;
        }

        // Set new parent using FReferenceSkeletonModifier
        // NewParentName can be empty/NAME_None to unparent (make root)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        FReferenceSkeletonModifier Modifier(Skeleton);
        FName ParentFName = NewParentName.IsEmpty() ? NAME_None : FName(*NewParentName);
        int32 NewBoneIndex = Modifier.SetParent(FName(*BoneName), ParentFName, true);

        if (NewBoneIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to set parent. New parent '%s' may not exist or operation invalid."), *NewParentName),
                TEXT("SET_PARENT_FAILED"));
            return true;
        }

        McpSafeAssetSave(Skeleton);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("newParent"), NewParentName.IsEmpty() ? TEXT("(none - root)") : NewParentName);
        Result->SetNumberField(TEXT("newBoneIndex"), NewBoneIndex);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' parent changed to '%s'"), *BoneName, NewParentName.IsEmpty() ? TEXT("(none)") : *NewParentName), Result);
        return true;
#else
        // UE 5.0-5.2: FReferenceSkeletonModifier doesn't have SetParent() method
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            TEXT("set_bone_parent is not supported in UE 5.0-5.2. Please use UE 5.3 or later."),
            TEXT("NOT_SUPPORTED"));
        return true;
#endif
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
