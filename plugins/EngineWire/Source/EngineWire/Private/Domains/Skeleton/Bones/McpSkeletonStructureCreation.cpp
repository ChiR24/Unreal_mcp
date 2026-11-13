#include "Domains/Skeleton/McpSkeletonActions.h"
#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Domains/Skeleton/Bones/McpSkeletonBoneEdits.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Animation/Skeleton.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"


namespace McpSkeletonHandlers {

bool HandleCreateSkeletonAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
        FString CreateError, CreateCode;
        UPackage* Package = McpCreatePackageForNewAsset(FullPackagePath, CreateError, CreateCode);
        if (!Package)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, CreateError, CreateCode);
            return true;
        }

        // Create skeleton asset
        USkeleton* NewSkeleton = NewObject<USkeleton>(Package, *SkeletonName, RF_Public | RF_Standalone);
        if (!NewSkeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create skeleton object"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Initialize with a root bone using FReferenceSkeletonModifier, which rebuilds the skeleton as it goes out of
        // scope: before the save below, not after it.
        {
            FReferenceSkeletonModifier Modifier(NewSkeleton);
            FMeshBoneInfo RootBone;
            RootBone.Name = FName(*RootBoneName);
            RootBone.ParentIndex = INDEX_NONE;
            RootBone.ExportName = RootBoneName;
            Modifier.Add(RootBone, FTransform::Identity);
        }
        McpSkeletonBoneEdits::FitBoneTree(NewSkeleton);

        const bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);
        const bool bSaveOk = !bSave || McpSafeAssetSave(NewSkeleton);
        if (!bSave || !bSaveOk)
        {
            NewSkeleton->MarkPackageDirty(); // unsaved, so save_all saves it
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        // A failed save (a /Temp path is never saved) answered like a saved skeleton.
        Result->SetBoolField(TEXT("saved"), bSave && bSaveOk);
        if (!bSaveOk)
        {
            TArray<TSharedPtr<FJsonValue>> Warnings;
            Warnings.Add(MakeShared<FJsonValueString>(TEXT("Created in memory only: the save failed, so it is lost when the editor closes unless control_editor save_all saves it.")));
            Result->SetArrayField(TEXT("warnings"), Warnings);
        }
        Result->SetStringField(TEXT("skeletonPath"), NewSkeleton->GetPathName());
        Result->SetStringField(TEXT("rootBoneName"), RootBoneName);
        Result->SetNumberField(TEXT("boneCount"), 1);
        // skeletonPath is no field a receipt reads, so the new skeleton was missing from it.
        McpHandlerUtils::AddVerification(Result, NewSkeleton);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Skeleton created with root bone '%s'"), *RootBoneName), Result);
        return true;
}

bool HandleAddBoneAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
            // A raw bone only: FindBoneIndex also finds the skeleton's virtual bones, which sit after the raw ones, and a
            // parent index that is not before the new bone's fails a check in FReferenceSkeleton::Add.
            ParentIndex = RefSkeleton.FindRawBoneIndex(FName(*ParentName));
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

        // The shared reader takes a uniform scale (scale: 2) as well as a vector; read as a vector only, it was dropped.
        FTransform BoneTransform = FTransform::Identity;
        ApplyTransformFieldsFromJson(Payload, BoneTransform);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        {
            // One undo step. The modifier rebuilds the skeleton when it goes out of scope, which came after the save
            // below: it ends here, so the skeleton written is the rebuilt one.
            FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Bone")), EMcpMutationDurability::EditorStateOnly, {Skeleton});
            FReferenceSkeletonModifier Modifier(Skeleton);
            FMeshBoneInfo NewBone;
            NewBone.Name = FName(*BoneName);
            NewBone.ParentIndex = ParentIndex;
            NewBone.ExportName = BoneName;

            // One root per skeleton holds: a parentless bone only goes into an empty one (PARENT_REQUIRED above).
            Modifier.Add(NewBone, BoneTransform);
            McpSkeletonBoneEdits::FitBoneTree(Skeleton);
            Transaction.DescribeInto(Result);
        }

        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("parentBone"), ParentName);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
        SaveAndReport(Skeleton, Payload, Result);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Bone '%s' added to skeleton"), *BoneName), Result);
        return true;
}

} // namespace McpSkeletonHandlers

