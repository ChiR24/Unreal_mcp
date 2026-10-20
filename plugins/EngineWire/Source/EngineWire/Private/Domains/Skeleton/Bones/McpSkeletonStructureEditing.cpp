#include "Domains/Skeleton/McpSkeletonActions.h"
#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"

#include "Animation/Skeleton.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "ReferenceSkeleton.h"


namespace McpSkeletonHandlers {

bool HandleRemoveBoneAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
        FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
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

        if (BoneIndex == 0)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot remove root bone"), TEXT("CANNOT_REMOVE_ROOT"));
            return true;
        }

        // Remove the bone using FReferenceSkeletonModifier
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Skeleton->Modify();
        FReferenceSkeletonModifier Modifier(Skeleton);
        Modifier.Remove(FName(*BoneName), bRemoveChildren);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("removedBone"), BoneName);
        Result->SetBoolField(TEXT("childrenRemoved"), bRemoveChildren);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
        SaveAndReport(Skeleton, Payload, Result);

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

bool HandleSetBoneParentAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
        FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
        // parentBoneName is the contract's name. Only the undeclared spellings
        // were read, so every gateway call arrived with an empty parent and the
        // bone was silently re-parented to a second root, reported as success.
        const FString NewParentName = McpGetFirstStringField(Payload, {TEXT("parentBoneName"), TEXT("parentBone"), TEXT("newParentBone")});

        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty() || NewParentName.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath, boneName and parentBoneName are required"), TEXT("MISSING_PARAM"));
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        const int32 NewParentIndex = RefSkeleton.FindBoneIndex(FName(*NewParentName));
        if (NewParentIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Parent bone '%s' not found (use list_bones)"), *NewParentName), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        // SetParent answers INDEX_NONE for the parent the bone already has and for one of its own descendants; both
        // used to be reported as "may not exist or operation invalid".
        if (RefSkeleton.GetParentIndex(BoneIndex) == NewParentIndex)
        {
            TSharedPtr<FJsonObject> Unchanged = McpHandlerUtils::CreateResultObject();
            Unchanged->SetStringField(TEXT("boneName"), BoneName);
            Unchanged->SetStringField(TEXT("newParent"), NewParentName);
            Unchanged->SetBoolField(TEXT("changed"), false);
            McpHandlerUtils::MarkNoAssetsChanged(Unchanged);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
                FString::Printf(TEXT("Bone '%s' already has the parent '%s'; nothing changed"), *BoneName, *NewParentName), Unchanged);
            return true;
        }
        if (NewParentIndex == BoneIndex || RefSkeleton.BoneIsChildOf(NewParentIndex, BoneIndex))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("'%s' is '%s' itself or one of its descendants; a bone cannot be parented under its own branch"), *NewParentName, *BoneName),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Skeleton->Modify();
        FReferenceSkeletonModifier Modifier(Skeleton);
        int32 NewBoneIndex = Modifier.SetParent(FName(*BoneName), FName(*NewParentName), false);

        if (NewBoneIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to set parent. New parent '%s' may not exist or operation invalid."), *NewParentName),
                TEXT("SET_PARENT_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("newParent"), NewParentName);
        Result->SetNumberField(TEXT("newBoneIndex"), NewBoneIndex);
        Result->SetBoolField(TEXT("changed"), true);
        SaveAndReport(Skeleton, Payload, Result);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' parent changed to '%s'"), *BoneName, *NewParentName), Result);
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

