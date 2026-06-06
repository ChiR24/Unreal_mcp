#include "McpAutomationBridge_LevelStructureActions.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpBridgeWebSocket.h"

static bool IsManageLevelStructureVolumeSubAction(const FString& SubAction)
{
    static const TSet<FString> VolumeSubActions = {
        TEXT("create_trigger_volume"),
        TEXT("add_trigger_volume"),
        TEXT("create_trigger_box"),
        TEXT("create_trigger_sphere"),
        TEXT("create_trigger_capsule"),
        TEXT("create_blocking_volume"),
        TEXT("add_blocking_volume"),
        TEXT("create_kill_z_volume"),
        TEXT("add_kill_z_volume"),
        TEXT("create_pain_causing_volume"),
        TEXT("create_physics_volume"),
        TEXT("add_physics_volume"),
        TEXT("create_audio_volume"),
        TEXT("create_reverb_volume"),
        TEXT("create_cull_distance_volume"),
        TEXT("add_cull_distance_volume"),
        TEXT("create_precomputed_visibility_volume"),
        TEXT("create_lightmass_importance_volume"),
        TEXT("create_nav_mesh_bounds_volume"),
        TEXT("create_nav_modifier_volume"),
        TEXT("create_camera_blocking_volume"),
        TEXT("create_post_process_volume"),
        TEXT("add_post_process_volume"),
        TEXT("set_volume_extent"),
        TEXT("set_volume_bounds"),
        TEXT("set_volume_properties"),
        TEXT("remove_volume"),
        TEXT("get_volumes_info")
    };

    return VolumeSubActions.Contains(SubAction);
}

bool UMcpAutomationBridgeSubsystem::HandleManageLevelStructureAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("subAction"), SubAction);
    }

    UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleManageLevelStructureAction: SubAction=%s"), *SubAction);

    bool bHandled = false;
    using namespace McpLevelStructure;

    if (SubAction == TEXT("create_level"))
    {
        bHandled = HandleCreateLevel(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_sublevel"))
    {
        bHandled = HandleCreateSublevel(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_level_streaming"))
    {
        bHandled = HandleConfigureLevelStreaming(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("set_streaming_distance"))
    {
        bHandled = HandleSetStreamingDistance(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_level_bounds"))
    {
        bHandled = HandleConfigureLevelBounds(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("enable_world_partition"))
    {
        bHandled = HandleEnableWorldPartition(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_grid_size"))
    {
        bHandled = HandleConfigureGridSize(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_data_layer"))
    {
        bHandled = HandleCreateDataLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("assign_actor_to_data_layer"))
    {
        bHandled = HandleAssignActorToDataLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_hlod_layer"))
    {
        bHandled = HandleConfigureHlodLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_minimap_volume"))
    {
        bHandled = HandleCreateMinimapVolume(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("open_level_blueprint"))
    {
        bHandled = HandleOpenLevelBlueprint(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("add_level_blueprint_node"))
    {
        bHandled = HandleAddLevelBlueprintNode(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("connect_level_blueprint_nodes"))
    {
        bHandled = HandleConnectLevelBlueprintNodes(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_level_instance"))
    {
        bHandled = HandleCreateLevelInstance(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_packed_level_actor"))
    {
        bHandled = HandleCreatePackedLevelActor(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("get_level_structure_info"))
    {
        bHandled = HandleGetLevelStructureInfo(this, RequestId, Payload, Socket);
    }
    else if (IsManageLevelStructureVolumeSubAction(SubAction))
    {
        bHandled = HandleManageVolumesAction(RequestId, Action, Payload, Socket);
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Unknown manage_level_structure action: %s"), *SubAction), nullptr);
        return true;
    }

    return bHandled;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("manage_level_structure requires editor build"), nullptr);
    return true;
#endif
}
