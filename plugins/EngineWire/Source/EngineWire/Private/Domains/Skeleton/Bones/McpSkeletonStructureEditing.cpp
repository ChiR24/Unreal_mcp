#include "Domains/Skeleton/McpSkeletonActions.h"
#include "Foundation/McpScopedEditorTransaction.h"
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

        // The engine refuses only the one root a skeleton keeps; a second root (add_bone allows several) was refused
        // too, as bone 0, though it removes like any other bone.
        int32 RootCount = 0;
        for (int32 Index = 0; Index < RefSkeleton.GetRawBoneNum(); ++Index)
        {
            RootCount += RefSkeleton.GetRawParentIndex(Index) == INDEX_NONE ? 1 : 0;
        }
        if (RefSkeleton.GetRawParentIndex(BoneIndex) == INDEX_NONE && RootCount <= 1)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot remove the skeleton's only root bone"), TEXT("CANNOT_REMOVE_ROOT"));
            return true;
        }

        // Remove the bone using FReferenceSkeletonModifier
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        {
            // One undo step; the modifier rebuilds the skeleton as it goes out of scope, before the save below.
            FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Bone")), EMcpMutationDurability::EditorStateOnly, {Skeleton});
            FReferenceSkeletonModifier Modifier(Skeleton);
            Modifier.Remove(FName(*BoneName), bRemoveChildren);
            Transaction.DescribeInto(Result);
        }

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
        int32 NewBoneIndex = INDEX_NONE;
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        {
            // One undo step; the modifier rebuilds the skeleton as it goes out of scope, before the save below.
            FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Bone Parent")), EMcpMutationDurability::EditorStateOnly, {Skeleton});
            FReferenceSkeletonModifier Modifier(Skeleton);
            NewBoneIndex = Modifier.SetParent(FName(*BoneName), FName(*NewParentName), false);
            Transaction.DescribeInto(Result);
        }

        if (NewBoneIndex == INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Failed to set parent. New parent '%s' may not exist or operation invalid."), *NewParentName),
                TEXT("SET_PARENT_FAILED"));
            return true;
        }

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

