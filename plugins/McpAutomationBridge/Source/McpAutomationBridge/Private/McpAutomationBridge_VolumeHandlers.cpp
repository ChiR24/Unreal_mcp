#include "McpAutomationBridge_VolumeActionDeclarations.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpVolumeHandlers, Log, All);

bool UMcpAutomationBridgeSubsystem::HandleManageVolumesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    UE_LOG(LogMcpVolumeHandlers, Verbose, TEXT("HandleManageVolumesAction: SubAction=%s"), *SubAction);
    using namespace McpVolumeHandlers;

    if (SubAction == TEXT("create_trigger_volume")) { return HandleCreateTriggerVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_trigger_box")) { return HandleCreateTriggerBox(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_trigger_sphere")) { return HandleCreateTriggerSphere(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_trigger_capsule")) { return HandleCreateTriggerCapsule(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_blocking_volume")) { return HandleCreateBlockingVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_kill_z_volume")) { return HandleCreateKillZVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_pain_causing_volume")) { return HandleCreatePainCausingVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_physics_volume")) { return HandleCreatePhysicsVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_audio_volume")) { return HandleCreateAudioVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_reverb_volume")) { return HandleCreateReverbVolume(this, RequestId, Payload, Socket); }

#if MCP_HAS_POSTPROCESS_VOLUME
    if (SubAction == TEXT("create_post_process_volume")) { return HandleCreatePostProcessVolume(this, RequestId, Payload, Socket); }
#else
    if (SubAction == TEXT("create_post_process_volume"))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("PostProcessVolume requires UE 5.1 or later"), nullptr, TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif
    if (SubAction == TEXT("create_cull_distance_volume")) { return HandleCreateCullDistanceVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_precomputed_visibility_volume")) { return HandleCreatePrecomputedVisibilityVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_lightmass_importance_volume")) { return HandleCreateLightmassImportanceVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_nav_mesh_bounds_volume")) { return HandleCreateNavMeshBoundsVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_nav_modifier_volume")) { return HandleCreateNavModifierVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("create_camera_blocking_volume")) { return HandleCreateCameraBlockingVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("set_volume_extent")) { return HandleSetVolumeExtent(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("set_volume_properties")) { return HandleSetVolumeProperties(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("set_volume_bounds")) { return HandleSetVolumeBounds(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("remove_volume")) { return HandleRemoveVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("get_volumes_info")) { return HandleGetVolumesInfo(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_trigger_volume")) { return HandleAddTriggerVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_blocking_volume")) { return HandleAddBlockingVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_kill_z_volume")) { return HandleAddKillZVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_physics_volume")) { return HandleAddPhysicsVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_cull_distance_volume")) { return HandleAddCullDistanceVolume(this, RequestId, Payload, Socket); }
#if MCP_HAS_POSTPROCESS_VOLUME
    if (SubAction == TEXT("add_post_process_volume")) { return HandleAddPostProcessVolume(this, RequestId, Payload, Socket); }
#else
    if (SubAction == TEXT("add_post_process_volume"))
    {
        SendAutomationResponse(Socket, RequestId, false, TEXT("PostProcessVolume requires UE 5.1 or later"), nullptr, TEXT("UNSUPPORTED_VERSION"));
        return true;
    }
#endif

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown volume subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("Volume operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
