#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"
#include "Domains/AnimationAuthoring/ControlRig/McpControlRigGraph.h"

#if MCP_HAS_CONTROLRIG_FACTORY
#include "ControlRigBlueprintGeneratedClass.h"
#endif

namespace McpAnimationAuthoring {

#if MCP_HAS_CONTROLRIG_FACTORY
UBlueprint* CreateControlRigBlueprint(
    const FString& AssetName,
    const FString& PackagePath,
    USkeleton* TargetSkeleton,
    FString& OutError,
    bool* bOutExisting)
{
    if (AssetName.IsEmpty())
    {
        OutError = TEXT("Asset name cannot be empty");
        return nullptr;
    }

    if (PackagePath.IsEmpty())
    {
        OutError = TEXT("Package path cannot be empty");
        return nullptr;
    }

    // Call the canonicalizer WHOLE rather than replaying its steps here. It
    // normalizes separators BEFORE mapping the /Content alias; doing it the
    // other way round left "\Content\TeamA\Thing" invisible to the alias map,
    // so this executor rooted it at /Game/Content/... while the pre-queue gate
    // had already resolved it to /Game/TeamA/... and admitted it. Guard and
    // executor now cannot disagree, because they run the same function.
    const FString NormalizedPath = McpCanonicalizeContentPath(PackagePath, /*bAssumeGameRoot=*/true);
    if (NormalizedPath.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("Package path is not a valid content path: %s"), *PackagePath);
        return nullptr;
    }

    const FString FullPackageName = NormalizedPath / AssetName;
    const FString FullObjectPath = FullPackageName + TEXT(".") + AssetName;
    // An object already at the path, in memory or on disk: reuse a Control Rig, refuse anything else.
    UObject* Existing = FindObject<UObject>(nullptr, *FullObjectPath);
    if (!Existing && FPackageName::DoesPackageExist(FullPackageName))
    {
        Existing = LoadObject<UObject>(nullptr, *FullObjectPath, nullptr, LOAD_NoWarn);
    }
    if (Existing)
    {
        if (Existing->IsA<UControlRigBlueprint>())
        {
            UE_LOG(LogEngineWireSubsystem, Log,
                TEXT("Control Rig Blueprint already exists, reusing: %s"), *FullObjectPath);
            if (bOutExisting)
            {
                *bOutExisting = true;
            }
            return Cast<UBlueprint>(Existing);
        }
        OutError = FString::Printf(
            TEXT("Asset exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *Existing->GetClass()->GetName());
        UE_LOG(LogEngineWireSubsystem, Error, TEXT("%s"), *OutError);
        return nullptr;
    }

    UPackage* Package = McpCreatePackageOrNull(FullPackageName);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName);
        return nullptr;
    }
    Package->FullyLoad();

    UControlRigBlueprint* NewBlueprint = Cast<UControlRigBlueprint>(
        FKismetEditorUtilities::CreateBlueprint(
            UControlRig::StaticClass(),
            Package,
            *AssetName,
            BPTYPE_Normal,
            UControlRigBlueprint::StaticClass(),
            UControlRigBlueprintGeneratedClass::StaticClass(),
            NAME_None));
    if (!NewBlueprint)
    {
        OutError = TEXT("Factory failed to create Control Rig Blueprint");
        return nullptr;
    }

    if (TargetSkeleton)
    {
        USkeletalMesh* PreviewMesh = TargetSkeleton->GetPreviewMesh();
        if (PreviewMesh)
        {
            NewBlueprint->SetPreviewMesh(PreviewMesh);
        }
    }

    FAssetRegistryModule::AssetCreated(NewBlueprint);
    NewBlueprint->MarkPackageDirty();
    McpSafeAssetSave(NewBlueprint);

    UE_LOG(
        LogEngineWireSubsystem,
        Log,
        TEXT("Created Control Rig Blueprint: %s"),
        *FullPackageName);
    return NewBlueprint;
}
#endif

TSharedPtr<FJsonObject> HandleControlRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_control_rig"))
    {
// ControlRig factory static methods (CreateNewControlRigAsset, CreateControlRigFromSkeletalMeshOrSkeleton)
// are only available in UE 5.5+ where ControlRigBlueprintFactory.h is in Public folder
#if MCP_HAS_CONTROLRIG_FACTORY && MCP_HAS_CONTROLRIG_BLUEPRINT && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
        bool bModularRig = GetJsonBoolField(Params, TEXT("modularRig"), false);
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        UControlRigBlueprint* ControlRigBP = nullptr;
        FString FullPath = Path / Name;

        UObject* SelectedObject = nullptr;
        if (!SkeletalMeshPath.IsEmpty())
        {
            SelectedObject = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SelectedObject)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
        }
        else if (!SkeletonPath.IsEmpty())
        {
            SelectedObject = LoadSkeletonFromPathAnim(SkeletonPath);
            if (!SelectedObject)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
            }
        }

        if (SelectedObject)
        {
            ControlRigBP = UControlRigBlueprintFactory::CreateControlRigFromSkeletalMeshOrSkeleton(SelectedObject, bModularRig);
        }
        else
        {
            ControlRigBP = UControlRigBlueprintFactory::CreateNewControlRigAsset(FullPath, bModularRig);
        }

        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig blueprint"), TEXT("CREATION_FAILED"));
        }

        // The skeleton factory names and places the asset after the skeleton;
        // move it to the requested path/name.
        if (ControlRigBP && !FullPath.IsEmpty() &&
            !ControlRigBP->GetOutermost()->GetName().Equals(FullPath, ESearchCase::IgnoreCase))
        {
            const FString OldObjectPath = ControlRigBP->GetPathName();
            if (UEditorAssetLibrary::RenameAsset(OldObjectPath, FullPath))
            {
                if (UControlRigBlueprint* Moved = Cast<UControlRigBlueprint>(McpLoadAsset(FullPath)))
                {
                    ControlRigBP = Moved;
                }
            }
        }
        if (!SaveAnimAsset(ControlRigBP, bSave))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save Control Rig blueprint"), TEXT("SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), bModularRig);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully"), *Name));
#elif MCP_HAS_CONTROLRIG_BLUEPRINT
        // Factory static methods not available in UE 5.1-5.4 (header is in Private folder)
        FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));

        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        // Create Control Rig Blueprint using FKismetEditorUtilities (works in all UE 5.x versions)
        const FString FullPackageName = Path / Name;

        // Create the package
        FString CreateError, CreateCode;
        UPackage* Package = McpCreatePackageForNewAsset(FullPackageName, CreateError, CreateCode);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(CreateError, CreateCode);
        }

        Package->FullyLoad();

        // Create the Control Rig Blueprint using FKismetEditorUtilities
        UControlRigBlueprint* ControlRigBP = Cast<UControlRigBlueprint>(
            FKismetEditorUtilities::CreateBlueprint(
                UControlRig::StaticClass(),
                Package,
                *Name,
                BPTYPE_Normal,
                UControlRigBlueprint::StaticClass(),
#if MCP_HAS_RIGVM_BLUEPRINT_GENERATED_CLASS
                URigVMBlueprintGeneratedClass::StaticClass(),
#else
                // UE 5.0-5.1 use UControlRigBlueprintGeneratedClass instead
                UControlRigBlueprintGeneratedClass::StaticClass(),
#endif
                NAME_None));

        if (!ControlRigBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create Control Rig Blueprint"), TEXT("CREATION_FAILED"));
        }

        // Set the target skeleton if provided (via skeletal mesh or skeleton path)
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SkeletalMesh)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
            if (SkeletalMesh->GetSkeleton())
            {
                USkeletalMesh* PreviewMesh = SkeletalMesh->GetSkeleton()->GetPreviewMesh();
                if (PreviewMesh)
                {
                    ControlRigBP->SetPreviewMesh(PreviewMesh);
                }
            }
        }
        else if (!SkeletonPath.IsEmpty())
        {
            USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
            if (!Skeleton)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
            }
            USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();
            if (PreviewMesh)
            {
                ControlRigBP->SetPreviewMesh(PreviewMesh);
            }
        }

        if (!SaveAnimAsset(ControlRigBP, GetJsonBoolField(Params, TEXT("save"), true)))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save Control Rig Blueprint"), TEXT("SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), false);  // Not supported in fallback
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully (UE 5.1-5.4 compatible mode)"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    return HandleControlRigGraphActions(SubAction, Params, Response);
}

} // namespace McpAnimationAuthoring
