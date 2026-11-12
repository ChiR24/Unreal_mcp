#include "Domains/Skeleton/McpSkeletonActions.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"


namespace McpSkeletonHandlers {

using FMemberHandler = bool (UEngineWireSubsystem::*)(
    const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
using FFreeHandler = bool (*)(
    UEngineWireSubsystem*, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

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
    UEngineWireSubsystem* Subsystem,
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
        {TEXT("prune_weights"), &HandlePruneWeightsAction},
        {TEXT("import_morph_targets"), &HandleImportMorphTargetsAction},
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

bool UEngineWireSubsystem::HandleManageSkeleton(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // animation_physics's route calls this with manage_skeleton and a payload whose sub-action it claimed.
    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);

    static const McpSkeletonHandlers::FSkeletonMemberRoute MemberRoutes[] = {
        {TEXT("get_skeleton_info"), &UEngineWireSubsystem::HandleGetSkeletonInfo},
        {TEXT("list_bones"), &UEngineWireSubsystem::HandleListBones},
        {TEXT("list_sockets"), &UEngineWireSubsystem::HandleListSockets},
        {TEXT("create_socket"), &UEngineWireSubsystem::HandleCreateSocket},
        {TEXT("add_socket"), &UEngineWireSubsystem::HandleCreateSocket},
        {TEXT("configure_socket"), &UEngineWireSubsystem::HandleConfigureSocket},
        {TEXT("modify_socket"), &UEngineWireSubsystem::HandleConfigureSocket},
        {TEXT("create_virtual_bone"), &UEngineWireSubsystem::HandleCreateVirtualBone},
        {TEXT("create_physics_asset"), &UEngineWireSubsystem::HandleCreatePhysicsAsset},
        {TEXT("list_physics_bodies"), &UEngineWireSubsystem::HandleListPhysicsBodies},
        {TEXT("add_physics_body"), &UEngineWireSubsystem::HandleAddPhysicsBody},
        {TEXT("configure_physics_body"), &UEngineWireSubsystem::HandleConfigurePhysicsBody},
        {TEXT("modify_physics_body"), &UEngineWireSubsystem::HandleConfigurePhysicsBody},
        {TEXT("add_physics_constraint"), &UEngineWireSubsystem::HandleAddPhysicsConstraint},
        {TEXT("configure_constraint_limits"), &UEngineWireSubsystem::HandleConfigureConstraintLimits},
        {TEXT("set_physics_asset"), &UEngineWireSubsystem::HandleSetPhysicsAsset},
        {TEXT("remove_physics_body"), &UEngineWireSubsystem::HandleRemovePhysicsBody},
        {TEXT("get_physics_asset_info"), &UEngineWireSubsystem::HandleGetPhysicsAssetInfo},
        {TEXT("rename_bone"), &UEngineWireSubsystem::HandleRenameBone},
        {TEXT("set_bone_transform"), &UEngineWireSubsystem::HandleSetBoneTransform},
        {TEXT("create_morph_target"), &UEngineWireSubsystem::HandleCreateMorphTarget},
        {TEXT("set_morph_target_deltas"), &UEngineWireSubsystem::HandleSetMorphTargetDeltas},
        {TEXT("set_morph_target_value"), &UEngineWireSubsystem::HandleSetMorphTargetValue},
        {TEXT("list_morph_targets"), &UEngineWireSubsystem::HandleListMorphTargets},
        {TEXT("delete_morph_target"), &UEngineWireSubsystem::HandleDeleteMorphTarget},
        {TEXT("remove_socket"), &UEngineWireSubsystem::HandleDeleteSocket},
        {TEXT("get_bone_transform"), &UEngineWireSubsystem::HandleGetBoneTransform},
        {TEXT("list_virtual_bones"), &UEngineWireSubsystem::HandleListVirtualBones},
        {TEXT("delete_virtual_bone"), &UEngineWireSubsystem::HandleDeleteVirtualBone},
        {TEXT("bind_cloth_to_skeletal_mesh"), &UEngineWireSubsystem::HandleBindClothToSkeletalMesh},
        {TEXT("assign_cloth_asset_to_mesh"), &UEngineWireSubsystem::HandleAssignClothAssetToMesh},
        {TEXT("set_physics_constraint"), &UEngineWireSubsystem::HandleAddPhysicsConstraint},
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

