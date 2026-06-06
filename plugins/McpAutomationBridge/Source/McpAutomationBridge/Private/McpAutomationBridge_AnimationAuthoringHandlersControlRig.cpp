#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleControlRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_control_rig"))
    {
// ControlRig factory static methods (CreateNewControlRigAsset, CreateControlRigFromSkeletalMeshOrSkeleton)
// are only available in UE 5.5+ where ControlRigBlueprintFactory.h is in Public folder
#if MCP_HAS_CONTROLRIG_FACTORY && MCP_HAS_CONTROLRIG_BLUEPRINT && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetStringFieldAnimAuth(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        bool bModularRig = GetBoolFieldAnimAuth(Params, TEXT("modularRig"), false);
    bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

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

        if (!SaveAnimAsset(ControlRigBP, bSave))
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to save Control Rig blueprint"), TEXT("SAVE_FAILED"));
        }

        Response->SetStringField(TEXT("assetPath"), ControlRigBP->GetPathName());
        Response->SetBoolField(TEXT("modularRig"), bModularRig);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Control Rig '%s' created successfully"), *Name));
#elif MCP_HAS_CONTROLRIG_BLUEPRINT
        // Factory static methods not available in UE 5.1-5.4 (header is in Private folder)
        // Use the Subsystem's CreateControlRigBlueprint method as fallback
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/ControlRigs")));
        FString SkeletalMeshPath = GetStringFieldAnimAuth(Params, TEXT("skeletalMeshPath"), TEXT(""));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));

        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        FString FullPath = Path / Name;

        // Create Control Rig Blueprint using FKismetEditorUtilities (works in all UE 5.x versions)
        FString FullPackageName = Path / Name;

        // Create the package
        UPackage* Package = CreatePackage(*FullPackageName);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName), TEXT("PACKAGE_CREATE_FAILED"));
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                URigVMBlueprintGeneratedClass::StaticClass(),
#else
                // UE 5.0 uses UControlRigBlueprintGeneratedClass instead
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

        if (!SaveAnimAsset(ControlRigBP, GetBoolFieldAnimAuth(Params, TEXT("save"), true)))
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

    if (SubAction == TEXT("add_control"))
    {
#if MCP_HAS_CONTROLRIG
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString ControlName = GetStringFieldAnimAuth(Params, TEXT("controlName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (ControlName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("controlName is required"), TEXT("MISSING_CONTROL_NAME"));
        }

        ANIM_ERROR_RESPONSE(
            TEXT("add_control is handled by the animation_physics runtime authoring route; call animation_physics with action=add_control."),
            TEXT("WRONG_HANDLER_ROUTE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    if (SubAction == TEXT("add_rig_unit"))
    {
#if MCP_HAS_CONTROLRIG
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString UnitType = GetStringFieldAnimAuth(Params, TEXT("unitType"), TEXT(""));

        ANIM_ERROR_RESPONSE(
            TEXT("add_rig_unit is handled by the animation_physics runtime authoring route; call animation_physics with action=add_rig_unit."),
            TEXT("WRONG_HANDLER_ROUTE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
