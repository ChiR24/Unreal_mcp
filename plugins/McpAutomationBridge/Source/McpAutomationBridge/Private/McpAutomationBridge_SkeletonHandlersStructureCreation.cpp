#include "McpAutomationBridge_SkeletonHandlersActions.h"
#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#if __has_include("Animation/SkeletonModifier.h")
#include "Animation/SkeletonModifier.h"
#endif

#if WITH_EDITOR

namespace McpSkeletonHandlers {

bool HandleCreateSkeletonAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("path"));
        if (SkeletonPath.IsEmpty())
        {
            SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        }
        FString RootBoneName = GetStringFieldSkel(Payload, TEXT("rootBoneName"));
        if (RootBoneName.IsEmpty())
        {
            RootBoneName = TEXT("Root");
        }

        if (SkeletonPath.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("path or skeletonPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // SECURITY: Validate path to prevent path traversal attacks
        // Ensure path starts with /Game/ and contains no traversal sequences
        if (!SkeletonPath.StartsWith(TEXT("/Game/")) && !SkeletonPath.StartsWith(TEXT("/Engine/")) && !SkeletonPath.StartsWith(TEXT("/Temp/")))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid path. Path must start with /Game/, /Engine/, or /Temp/"), TEXT("INVALID_PATH"));
            return true;
        }

        // Check for path traversal attempts
        if (SkeletonPath.Contains(TEXT("..")) || SkeletonPath.Contains(TEXT("//")) || SkeletonPath.Contains(TEXT("\\")))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid path. Path contains illegal characters or traversal sequences"), TEXT("INVALID_PATH"));
            return true;
        }

        // Validate using Unreal's package name validation (UE 5.7 uses EErrorCode)
        FPackageName::EErrorCode ErrorCode;
        if (!FPackageName::IsValidLongPackageName(SkeletonPath, false, &ErrorCode))
        {
            FString ErrorMsg = FPackageName::FormatErrorAsString(SkeletonPath, ErrorCode);
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid package path: %s"), *ErrorMsg), TEXT("INVALID_PATH"));
            return true;
        }

        // Normalize path
        FString PackagePath = FPaths::GetPath(SkeletonPath);
        FString SkeletonName = FPaths::GetBaseFilename(SkeletonPath);
        FString FullPackagePath = PackagePath / SkeletonName;

        // Create package
        UPackage* Package = CreatePackage(*FullPackagePath);
        if (!Package)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Create skeleton asset
        USkeleton* NewSkeleton = NewObject<USkeleton>(Package, *SkeletonName, RF_Public | RF_Standalone);
        if (!NewSkeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create skeleton object"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Initialize with a root bone using FReferenceSkeletonModifier
        FReferenceSkeletonModifier Modifier(NewSkeleton);
        FMeshBoneInfo RootBone;
        RootBone.Name = FName(*RootBoneName);
        RootBone.ParentIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
        RootBone.ExportName = RootBoneName;
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Modifier.Add(RootBone, FTransform::Identity, true); // bAllowMultipleRoots = true for first bone
#else
        // UE 5.0-5.2: Add() only takes 2 parameters
        Modifier.Add(RootBone, FTransform::Identity);
#endif

        McpSafeAssetSave(NewSkeleton);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletonPath"), NewSkeleton->GetPathName());
        Result->SetStringField(TEXT("rootBoneName"), RootBoneName);
        Result->SetNumberField(TEXT("boneCount"), 1);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Skeleton created with root bone '%s'"), *RootBoneName), Result);
        return true;
}

bool HandleAddBoneAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        FString ParentName = GetStringFieldSkel(Payload, TEXT("parentBone"));
        if (ParentName.IsEmpty())
        {
            ParentName = GetStringFieldSkel(Payload, TEXT("parentBoneName"));
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

        // Check if bone already exists
        if (RefSkeleton.FindBoneIndex(FName(*BoneName)) != INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Bone '%s' already exists"), *BoneName), TEXT("BONE_EXISTS"));
            return true;
        }

        // Find parent bone index
        int32 ParentIndex = INDEX_NONE;
        if (!ParentName.IsEmpty())
        {
            ParentIndex = RefSkeleton.FindBoneIndex(FName(*ParentName));
            if (ParentIndex == INDEX_NONE)
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Parent bone '%s' not found"), *ParentName), TEXT("PARENT_NOT_FOUND"));
                return true;
            }
        }
        else if (RefSkeleton.GetRawBoneNum() > 0)
        {
            // Cannot add a root bone if the skeleton already has bones - need to specify a parent
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Cannot add root bone; Skeleton already has bones. Specify parentBone."), TEXT("PARENT_REQUIRED"));
            return true;
        }

        // Parse transform from payload
        FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
        FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
        FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);
        FTransform BoneTransform(Rotation, Location, Scale);

        // Add the bone using FReferenceSkeletonModifier
        FReferenceSkeletonModifier Modifier(Skeleton);
        FMeshBoneInfo NewBone;
        NewBone.Name = FName(*BoneName);
        NewBone.ParentIndex = ParentIndex;
#if WITH_EDITORONLY_DATA
        NewBone.ExportName = BoneName;
#endif

        // Allow multiple roots only if no parent is specified and this is the first bone
        bool bAllowMultipleRoots = ParentIndex == INDEX_NONE && RefSkeleton.GetRawBoneNum() == 0;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Modifier.Add(NewBone, BoneTransform, bAllowMultipleRoots);
#else
        // UE 5.0-5.2: Add() only takes 2 parameters
        Modifier.Add(NewBone, BoneTransform);
#endif

        McpSafeAssetSave(Skeleton);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("parentBone"), ParentName);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' added to skeleton"), *BoneName), Result);
        return true;
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
