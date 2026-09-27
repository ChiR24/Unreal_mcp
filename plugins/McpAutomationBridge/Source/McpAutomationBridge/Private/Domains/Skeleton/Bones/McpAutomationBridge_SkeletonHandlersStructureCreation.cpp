#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"


namespace McpSkeletonHandlers {

bool HandleCreateSkeletonAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletonPath = GetJsonStringField(Payload, TEXT("path"));
        if (SkeletonPath.IsEmpty())
        {
            SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
        }
        // `name` is published on this action's contract and was never read: a
        // call with name="SK_AuditRig", path="/Game/McpAudit" created
        // "/Game/McpAudit.McpAudit" -- an asset named after the FOLDER, sitting
        // beside it and shadowing it -- and reported that path back as success.
        // Treat `path` as a folder when a name is supplied and it does not
        // already end in that name, so both spellings land in the right place.
        const FString AssetName = GetJsonStringField(Payload, TEXT("name"));
        if (!AssetName.IsEmpty() && !SkeletonPath.IsEmpty() &&
            !SkeletonPath.EndsWith(FString(TEXT("/")) + AssetName))
        {
            SkeletonPath.RemoveFromEnd(TEXT("/"));
            SkeletonPath = SkeletonPath + TEXT("/") + AssetName;
        }
        FString RootBoneName = GetJsonStringField(Payload, TEXT("rootBoneName"));
        if (RootBoneName.IsEmpty())
        {
            RootBoneName = TEXT("Root");
        }

        if (SkeletonPath.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("path or skeletonPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // SECURITY: the shared canonicalizer refuses traversal and colons; the skeleton itself
        // may only be created under /Game, /Engine or /Temp.
        SkeletonPath = McpCanonicalizeContentPath(SkeletonPath);
        if (!SkeletonPath.StartsWith(TEXT("/Game/")) && !SkeletonPath.StartsWith(TEXT("/Engine/")) && !SkeletonPath.StartsWith(TEXT("/Temp/")))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Invalid path. Path must be a traversal-free path under /Game/, /Engine/, or /Temp/"), TEXT("INVALID_PATH"));
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
        RootBone.ExportName = RootBoneName;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Modifier.Add(RootBone, FTransform::Identity, true); // bAllowMultipleRoots = true for first bone
#else
        // UE 5.0-5.2: Add() only takes 2 parameters
        Modifier.Add(RootBone, FTransform::Identity);
#endif

        SaveIfRequested(NewSkeleton, Payload);

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
        FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
        FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
        // The declared parentBoneName wins over the older parentBone spelling.
        const FString ParentName = McpGetFirstStringField(Payload, {TEXT("parentBoneName"), TEXT("parentBone")});

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

        if (RefSkeleton.FindBoneIndex(FName(*BoneName)) != INDEX_NONE)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Bone '%s' already exists"), *BoneName), TEXT("BONE_EXISTS"));
            return true;
        }

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
                TEXT("Cannot add root bone; Skeleton already has bones. Specify parentBoneName."), TEXT("PARENT_REQUIRED"));
            return true;
        }

        FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
        FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
        FVector Scale = ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);
        FTransform BoneTransform(Rotation, Location, Scale);

        FReferenceSkeletonModifier Modifier(Skeleton);
        FMeshBoneInfo NewBone;
        NewBone.Name = FName(*BoneName);
        NewBone.ParentIndex = ParentIndex;
        NewBone.ExportName = BoneName;

        // Allow multiple roots only if no parent is specified and this is the first bone
        bool bAllowMultipleRoots = ParentIndex == INDEX_NONE && RefSkeleton.GetRawBoneNum() == 0;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        Modifier.Add(NewBone, BoneTransform, bAllowMultipleRoots);
#else
        // UE 5.0-5.2: Add() only takes 2 parameters
        Modifier.Add(NewBone, BoneTransform);
#endif

        SaveIfRequested(Skeleton, Payload);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("parentBone"), ParentName);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' added to skeleton"), *BoneName), Result);
        return true;
}

} // namespace McpSkeletonHandlers

