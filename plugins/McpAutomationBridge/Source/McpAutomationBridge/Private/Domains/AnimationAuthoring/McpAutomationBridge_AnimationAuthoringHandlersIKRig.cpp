#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleIKRigActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
if (SubAction == TEXT("create_ik_rig"))
{
#if MCP_HAS_IKRIG_FACTORY && MCP_HAS_IKRIG
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Retargeting")));
    FString SkeletalMeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    // Use the static factory (UE 5.6+, it notifies the asset registry) or fall
    // back to NewObject on older engines where CreateNewIKRigAsset is absent.
#if MCP_HAS_IKRIG_CREATE_NEW_ASSET
    UIKRigDefinition* IKRig = MCP_IKRIG_CREATE_NEW_ASSET(Path, Name);
#else
    // UE 5.0-5.5: Create using NewObject since CreateNewIKRigAsset doesn't exist
    UPackage* Package = CreatePackage(*FString(Path / Name));
    if (!Package)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create package for IK Rig"), TEXT("PACKAGE_FAILED"));
    }
    UIKRigDefinition* IKRig = NewObject<UIKRigDefinition>(Package, *Name, RF_Public | RF_Standalone);
    if (!IKRig)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Rig asset"), TEXT("CREATION_FAILED"));
    }
    // CreateNewIKRigAsset notifies the asset registry for us; NewObject does not.
    // Without this the rig exists on disk but is unregistered, so it does not
    // appear in the Content Browser until an unrelated rescan happens to pick it
    // up — the asset looked lost even though creation had reported success.
    FAssetRegistryModule::AssetCreated(IKRig);
    // Mark the package as needing save
    Package->MarkPackageDirty();
#endif

    if (!IKRig)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create IK Rig asset"), TEXT("CREATION_FAILED"));
    }

        // If skeletal mesh path provided, set the preview mesh
        if (!SkeletalMeshPath.IsEmpty())
        {
            USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathAnim(SkeletalMeshPath);
            if (!SkeletalMesh)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeletal mesh: %s"), *SkeletalMeshPath), TEXT("SKELETAL_MESH_NOT_FOUND"));
            }
            IKRig->SetPreviewMesh(SkeletalMesh);
        }
        // Also support skeletonPath: load skeleton and set its preview mesh on the IK Rig
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
                IKRig->SetPreviewMesh(PreviewMesh);
            }
            Response->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());
        }

    if (!SaveAnimAsset(IKRig, bSave))
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to save IK Rig asset"), TEXT("SAVE_FAILED"));
    }

    Response->SetStringField(TEXT("assetPath"), IKRig->GetPathName());
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("IK Rig '%s' created successfully"), *Name));
    return Response;
#elif MCP_HAS_IKRIG
    ANIM_ERROR_RESPONSE(
        TEXT("create_ik_rig requires the IKRigEditor factory module in this build"),
        TEXT("IKRIG_FACTORY_UNAVAILABLE"));
#else
    ANIM_ERROR_RESPONSE(TEXT("IK Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
}

    if (SubAction == TEXT("add_ik_chain"))
    {
#if MCP_HAS_IKRIG
        FString ChainName = GetJsonStringField(Params, TEXT("chainName"), TEXT(""));

        if (ChainName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("chainName is required"), TEXT("MISSING_CHAIN_NAME"));
        }

        ANIM_ERROR_RESPONSE(
            TEXT("add_ik_chain is handled by the animation_physics runtime authoring route; call animation_physics with action=add_ik_chain."),
            TEXT("WRONG_HANDLER_ROUTE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("IK Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
