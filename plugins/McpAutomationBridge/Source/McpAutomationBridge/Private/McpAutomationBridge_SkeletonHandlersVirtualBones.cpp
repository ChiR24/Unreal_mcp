#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleCreateVirtualBone(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString SourceBone = GetStringFieldSkel(Payload, TEXT("sourceBoneName"));
    FString TargetBone = GetStringFieldSkel(Payload, TEXT("targetBoneName"));
    FString VirtualBoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (SkeletonPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    if (SourceBone.IsEmpty() || TargetBone.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("sourceBoneName and targetBoneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Generate virtual bone name if not provided
    if (VirtualBoneName.IsEmpty())
    {
        VirtualBoneName = FString::Printf(TEXT("VB_%s_to_%s"), *SourceBone, *TargetBone);
    }

    // Add virtual bone
    FName NewVirtualBoneName;
    bool bSuccess = Skeleton->AddNewVirtualBone(FName(*SourceBone), FName(*TargetBone), NewVirtualBoneName);

    if (!bSuccess)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to create virtual bone. Check that source and target bones exist."),
            TEXT("VIRTUAL_BONE_FAILED"));
        return true;
    }

    // Rename if custom name provided
    if (!VirtualBoneName.IsEmpty() && NewVirtualBoneName.ToString() != VirtualBoneName)
    {
        Skeleton->RenameVirtualBone(NewVirtualBoneName, FName(*VirtualBoneName));
        NewVirtualBoneName = FName(*VirtualBoneName);
    }

    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("virtualBoneName"), NewVirtualBoneName.ToString());
    Result->SetStringField(TEXT("sourceBone"), SourceBone);
    Result->SetStringField(TEXT("targetBone"), TargetBone);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Virtual bone '%s' created"), *NewVirtualBoneName.ToString()), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleRenameBone(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    FString NewBoneName = GetStringFieldSkel(Payload, TEXT("newBoneName"));

    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty() || NewBoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletonPath, boneName, and newBoneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Check if it's a virtual bone
    const TArray<FVirtualBone>& VirtualBones = Skeleton->GetVirtualBones();
    bool bIsVirtualBone = false;
    for (const FVirtualBone& VB : VirtualBones)
    {
        if (VB.VirtualBoneName == FName(*BoneName))
        {
            bIsVirtualBone = true;
            break;
        }
    }

    if (bIsVirtualBone)
    {
        Skeleton->RenameVirtualBone(FName(*BoneName), FName(*NewBoneName));
        McpSafeAssetSave(Skeleton);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave)
        {
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("oldName"), BoneName);
        Result->SetStringField(TEXT("newName"), NewBoneName);
        Result->SetBoolField(TEXT("isVirtualBone"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Virtual bone renamed from '%s' to '%s'"), *BoneName, *NewBoneName), Result);
        return true;
    }

    // For regular bones, renaming is not directly supported without reimporting
    // We can rename bone mappings in animation assets though
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Renaming non-virtual bones is not supported. Only virtual bones can be renamed at runtime. To rename regular bones, reimport the skeletal mesh with updated bone names."),
        TEXT("OPERATION_NOT_SUPPORTED"));
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleListVirtualBones(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

    USkeleton* Skeleton = nullptr;

    if (!SkeletonPath.IsEmpty())
    {
        FString Error;
        Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return true;
        }
    }
    else if (!SkeletalMeshPath.IsEmpty())
    {
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return true;
        }
        Skeleton = Mesh->GetSkeleton();
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletonPath or skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> VirtualBoneArray;
    for (const FVirtualBone& VB : Skeleton->GetVirtualBones())
    {
        TSharedPtr<FJsonObject> VBObj = McpHandlerUtils::CreateResultObject();
        VBObj->SetStringField(TEXT("name"), VB.VirtualBoneName.ToString());
        VBObj->SetStringField(TEXT("sourceBone"), VB.SourceBoneName.ToString());
        VBObj->SetStringField(TEXT("targetBone"), VB.TargetBoneName.ToString());
        VirtualBoneArray.Add(MakeShared<FJsonValueObject>(VBObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
    Result->SetArrayField(TEXT("virtualBones"), VirtualBoneArray);
    Result->SetNumberField(TEXT("count"), VirtualBoneArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Found %d virtual bones"), VirtualBoneArray.Num()), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("list_virtual_bones requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDeleteVirtualBone(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString VirtualBoneName = GetStringFieldSkel(Payload, TEXT("virtualBoneName"));

    if (SkeletonPath.IsEmpty() || VirtualBoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletonPath and virtualBoneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return true;
    }

    // Find and remove the virtual bone
    const TArray<FVirtualBone>& VirtualBones = Skeleton->GetVirtualBones();
    int32 FoundIndex = INDEX_NONE;
    for (int32 i = 0; i < VirtualBones.Num(); ++i)
    {
        if (VirtualBones[i].VirtualBoneName == FName(*VirtualBoneName))
        {
            FoundIndex = i;
            break;
        }
    }

    if (FoundIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Virtual bone '%s' not found"), *VirtualBoneName), TEXT("VBONE_NOT_FOUND"));
        return true;
    }

    // Remove using the skeleton's API
    TArray<FName> BonesToRemove;
    BonesToRemove.Add(FName(*VirtualBoneName));
    Skeleton->RemoveVirtualBones(BonesToRemove);
    McpSafeAssetSave(Skeleton);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletonPath"), SkeletonPath);
    Result->SetStringField(TEXT("virtualBoneName"), VirtualBoneName);
    Result->SetNumberField(TEXT("remainingVirtualBones"), Skeleton->GetVirtualBones().Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Virtual bone '%s' deleted"), *VirtualBoneName), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("delete_virtual_bone requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
