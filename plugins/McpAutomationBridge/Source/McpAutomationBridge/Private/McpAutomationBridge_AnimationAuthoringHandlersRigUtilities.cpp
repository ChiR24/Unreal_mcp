#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleRigUtilityActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("connect_rig_elements"))
    {
#if MCP_HAS_CONTROLRIG
        ANIM_ERROR_RESPONSE(
            TEXT("connect_rig_elements is handled by the animation_physics runtime authoring route; call animation_physics with action=connect_rig_elements."),
            TEXT("WRONG_HANDLER_ROUTE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("Control Rig module not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    if (SubAction == TEXT("create_pose_library"))
    {
#if MCP_HAS_POSEASSET
        FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
        FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (Name.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
        }

        USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }

        ANIM_ERROR_RESPONSE(
            TEXT("create_pose_library is handled by the animation_physics runtime authoring route; call animation_physics with action=create_pose_library."),
            TEXT("WRONG_HANDLER_ROUTE"));
#else
        ANIM_ERROR_RESPONSE(TEXT("Pose Asset not available in this engine version"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    // ===== 10.6 Retargeting =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
