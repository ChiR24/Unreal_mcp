#include "McpAutomationBridge_SkeletonHandlersActions.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"

#if WITH_EDITOR

namespace McpSkeletonHandlers {

using FMemberHandler = bool (UMcpAutomationBridgeSubsystem::*)(
    const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
using FFreeHandler = bool (*)(
    UMcpAutomationBridgeSubsystem*, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

struct FSkeletonMemberRoute
{
    const TCHAR* SubAction;
    FMemberHandler Handler;
};

struct FSkeletonFreeRoute
{
    const TCHAR* SubAction;
    FFreeHandler Handler;
};

static bool TryHandleFreeRoute(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    static const FSkeletonFreeRoute Routes[] = {
        {TEXT("create_skeleton"), &HandleCreateSkeletonAction},
        {TEXT("add_bone"), &HandleAddBoneAction},
        {TEXT("remove_bone"), &HandleRemoveBoneAction},
        {TEXT("set_bone_parent"), &HandleSetBoneParentAction},
        {TEXT("set_vertex_weights"), &HandleSetVertexWeightsAction},
        {TEXT("auto_skin_weights"), &HandleAutoSkinWeightsAction},
        {TEXT("copy_weights"), &HandleCopyWeightsAction},
        {TEXT("mirror_weights"), &HandleMirrorWeightsAction},
        {TEXT("preview_physics"), &HandlePreviewPhysicsAction},
    };

    for (const FSkeletonFreeRoute& Route : Routes)
    {
        if (SubAction == Route.SubAction)
        {
            return Route.Handler(Subsystem, RequestId, Payload, RequestingSocket);
        }
    }

    return false;
}

} // namespace McpSkeletonHandlers

bool UMcpAutomationBridgeSubsystem::HandleManageSkeleton(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_skeleton"))
    {
        return true;
    }

    FString SubAction;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Skeleton action (subAction) is required"), TEXT("MISSING_ACTION"));
        return true;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleManageSkeleton: %s"), *SubAction);

    static const McpSkeletonHandlers::FSkeletonMemberRoute MemberRoutes[] = {
        {TEXT("get_skeleton_info"), &UMcpAutomationBridgeSubsystem::HandleGetSkeletonInfo},
        {TEXT("list_bones"), &UMcpAutomationBridgeSubsystem::HandleListBones},
        {TEXT("list_sockets"), &UMcpAutomationBridgeSubsystem::HandleListSockets},
        {TEXT("create_socket"), &UMcpAutomationBridgeSubsystem::HandleCreateSocket},
        {TEXT("add_socket"), &UMcpAutomationBridgeSubsystem::HandleCreateSocket},
        {TEXT("configure_socket"), &UMcpAutomationBridgeSubsystem::HandleConfigureSocket},
        {TEXT("modify_socket"), &UMcpAutomationBridgeSubsystem::HandleConfigureSocket},
        {TEXT("create_virtual_bone"), &UMcpAutomationBridgeSubsystem::HandleCreateVirtualBone},
        {TEXT("create_physics_asset"), &UMcpAutomationBridgeSubsystem::HandleCreatePhysicsAsset},
        {TEXT("list_physics_bodies"), &UMcpAutomationBridgeSubsystem::HandleListPhysicsBodies},
        {TEXT("add_physics_body"), &UMcpAutomationBridgeSubsystem::HandleAddPhysicsBody},
        {TEXT("configure_physics_body"), &UMcpAutomationBridgeSubsystem::HandleConfigurePhysicsBody},
        {TEXT("modify_physics_body"), &UMcpAutomationBridgeSubsystem::HandleConfigurePhysicsBody},
        {TEXT("add_physics_constraint"), &UMcpAutomationBridgeSubsystem::HandleAddPhysicsConstraint},
        {TEXT("configure_constraint_limits"), &UMcpAutomationBridgeSubsystem::HandleConfigureConstraintLimits},
        {TEXT("set_physics_asset"), &UMcpAutomationBridgeSubsystem::HandleSetPhysicsAsset},
        {TEXT("remove_physics_body"), &UMcpAutomationBridgeSubsystem::HandleRemovePhysicsBody},
        {TEXT("get_physics_asset_info"), &UMcpAutomationBridgeSubsystem::HandleGetPhysicsAssetInfo},
        {TEXT("rename_bone"), &UMcpAutomationBridgeSubsystem::HandleRenameBone},
        {TEXT("set_bone_transform"), &UMcpAutomationBridgeSubsystem::HandleSetBoneTransform},
        {TEXT("create_morph_target"), &UMcpAutomationBridgeSubsystem::HandleCreateMorphTarget},
        {TEXT("set_morph_target_deltas"), &UMcpAutomationBridgeSubsystem::HandleSetMorphTargetDeltas},
        {TEXT("import_morph_targets"), &UMcpAutomationBridgeSubsystem::HandleImportMorphTargets},
        {TEXT("set_morph_target_value"), &UMcpAutomationBridgeSubsystem::HandleSetMorphTargetValue},
        {TEXT("list_morph_targets"), &UMcpAutomationBridgeSubsystem::HandleListMorphTargets},
        {TEXT("delete_morph_target"), &UMcpAutomationBridgeSubsystem::HandleDeleteMorphTarget},
        {TEXT("delete_socket"), &UMcpAutomationBridgeSubsystem::HandleDeleteSocket},
        {TEXT("remove_socket"), &UMcpAutomationBridgeSubsystem::HandleDeleteSocket},
        {TEXT("get_bone_transform"), &UMcpAutomationBridgeSubsystem::HandleGetBoneTransform},
        {TEXT("list_virtual_bones"), &UMcpAutomationBridgeSubsystem::HandleListVirtualBones},
        {TEXT("delete_virtual_bone"), &UMcpAutomationBridgeSubsystem::HandleDeleteVirtualBone},
        {TEXT("normalize_weights"), &UMcpAutomationBridgeSubsystem::HandleNormalizeWeights},
        {TEXT("prune_weights"), &UMcpAutomationBridgeSubsystem::HandlePruneWeights},
        {TEXT("bind_cloth_to_skeletal_mesh"), &UMcpAutomationBridgeSubsystem::HandleBindClothToSkeletalMesh},
        {TEXT("assign_cloth_asset_to_mesh"), &UMcpAutomationBridgeSubsystem::HandleAssignClothAssetToMesh},
        {TEXT("set_physics_constraint"), &UMcpAutomationBridgeSubsystem::HandleAddPhysicsConstraint},
    };

    for (const McpSkeletonHandlers::FSkeletonMemberRoute& Route : MemberRoutes)
    {
        if (SubAction == Route.SubAction)
        {
            if ((this->*Route.Handler)(RequestId, Payload, RequestingSocket))
            {
                return true;
            }
            break;
        }
    }

    if (McpSkeletonHandlers::TryHandleFreeRoute(this, RequestId, SubAction, Payload, RequestingSocket))
    {
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown skeleton action: %s"), *SubAction),
        TEXT("UNKNOWN_ACTION"));
    return true;
}

#endif // WITH_EDITOR
