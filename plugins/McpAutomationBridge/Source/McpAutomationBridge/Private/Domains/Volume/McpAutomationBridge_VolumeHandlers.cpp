#include "Domains/Volume/McpAutomationBridge_VolumeActionDeclarations.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpVolumeHandlers, Log, All);

namespace
{
// create_volume declares one parameter union for every class; each class honours only its own share. A
// parameter the chosen class would ignore is refused instead of dropped (a sphereRadius on a BlockingVolume
// used to answer success). Keys are the create_/add_ action minus its prefix.
const TCHAR* const McpVolumeClassSpecificParams[] = {
    TEXT("extent"), TEXT("actorPath"), TEXT("boxExtent"), TEXT("sphereRadius"), TEXT("capsuleRadius"), TEXT("capsuleHalfHeight"),
    TEXT("killZHeight"), TEXT("bPainCausing"), TEXT("damagePerSec"), TEXT("bWaterVolume"), TEXT("fluidFriction"),
    TEXT("terminalVelocity"), TEXT("priority"), TEXT("bEnabled"), TEXT("reverbVolume"), TEXT("fadeTime"), TEXT("cullDistances"),
    TEXT("bUnbound"), TEXT("blendRadius"), TEXT("blendWeight"), TEXT("postProcessSettings")};

const TMap<FString, FString>& McpVolumeParamsByClass()
{
    static const TMap<FString, FString> Params = {
        {TEXT("trigger_volume"), TEXT("extent actorPath")},
        {TEXT("trigger_box"), TEXT("extent boxExtent")},
        {TEXT("trigger_sphere"), TEXT("sphereRadius")},
        {TEXT("trigger_capsule"), TEXT("capsuleRadius capsuleHalfHeight")},
        {TEXT("blocking_volume"), TEXT("extent actorPath")},
        {TEXT("kill_z_volume"), TEXT("extent actorPath killZHeight")},
        {TEXT("pain_causing_volume"), TEXT("extent bPainCausing damagePerSec")},
        {TEXT("physics_volume"), TEXT("extent actorPath bWaterVolume fluidFriction terminalVelocity priority")},
        {TEXT("audio_volume"), TEXT("extent bEnabled")},
        {TEXT("reverb_volume"), TEXT("extent bEnabled reverbVolume fadeTime")},
        {TEXT("cull_distance_volume"), TEXT("extent actorPath cullDistances")},
        {TEXT("precomputed_visibility_volume"), TEXT("extent")},
        {TEXT("lightmass_importance_volume"), TEXT("extent")},
        {TEXT("nav_mesh_bounds_volume"), TEXT("extent")},
        {TEXT("nav_modifier_volume"), TEXT("extent")},
        {TEXT("camera_blocking_volume"), TEXT("extent")},
        {TEXT("post_process_volume"), TEXT("extent actorPath priority bEnabled bUnbound blendRadius blendWeight postProcessSettings")}};
    return Params;
}

// The first passed class-specific parameter ClassKey ignores, or empty; OutAllowed lists what it honours.
FString McpFindInapplicableVolumeParam(const FString& ClassKey, const TSharedPtr<FJsonObject>& Payload, FString& OutAllowed)
{
    const FString* Allowed = McpVolumeParamsByClass().Find(ClassKey);
    if (!Allowed || !Payload.IsValid())
    {
        return FString();
    }
    TArray<FString> AllowedNames;
    Allowed->ParseIntoArray(AllowedNames, TEXT(" "));
    OutAllowed = FString::Join(AllowedNames, TEXT(", "));
    for (const TCHAR* Param : McpVolumeClassSpecificParams)
    {
        if (Payload->HasField(Param) && !AllowedNames.Contains(Param))
        {
            return Param;
        }
    }
    return FString();
}
}

bool UMcpAutomationBridgeSubsystem::HandleManageVolumesAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& InPayload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> Payload = InPayload;
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));
    UE_LOG(LogMcpVolumeHandlers, Verbose, TEXT("HandleManageVolumesAction: SubAction=%s"), *SubAction);
    using namespace McpVolumeHandlers;

    const FString ClassKey = SubAction.StartsWith(TEXT("create_")) ? SubAction.Mid(7)
        : (SubAction.StartsWith(TEXT("add_")) ? SubAction.Mid(4) : FString());
    FString AllowedParams;
    const FString Inapplicable = McpFindInapplicableVolumeParam(ClassKey, Payload, AllowedParams);
    if (!Inapplicable.IsEmpty())
    {
        SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("%s does not apply to %s; that volume class takes: %s. Nothing was created."), *Inapplicable, *ClassKey, *AllowedParams),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // actorPath attaches the volume; a create_ name with actorPath takes the add_ route that honours it.
    if (SubAction.StartsWith(TEXT("create_")) && Payload.IsValid() && Payload->HasField(TEXT("actorPath")))
    {
        SubAction = TEXT("add_") + ClassKey;
    }
    // killZHeight is the kill plane: on the standalone create route it sets the volume's Z.
    if (ClassKey == TEXT("kill_z_volume") && Payload.IsValid() && Payload->HasField(TEXT("killZHeight")) && !Payload->HasField(TEXT("actorPath")))
    {
        Payload = MakeShared<FJsonObject>(*InPayload);
        FVector Location = ExtractVectorField(InPayload, TEXT("location"), FVector::ZeroVector);
        Location.Z = GetJsonNumberField(InPayload, TEXT("killZHeight"), Location.Z);
        TSharedPtr<FJsonObject> LocationJson = MakeShared<FJsonObject>();
        LocationJson->SetNumberField(TEXT("x"), Location.X);
        LocationJson->SetNumberField(TEXT("y"), Location.Y);
        LocationJson->SetNumberField(TEXT("z"), Location.Z);
        Payload->SetObjectField(TEXT("location"), LocationJson);
    }

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
    // The add_* names are catalogued as aliases of create_*; only an actorPath makes them attach to an actor.
    if (SubAction == TEXT("add_trigger_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddTriggerVolume(this, RequestId, Payload, Socket) : HandleCreateTriggerVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_blocking_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddBlockingVolume(this, RequestId, Payload, Socket) : HandleCreateBlockingVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_kill_z_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddKillZVolume(this, RequestId, Payload, Socket) : HandleCreateKillZVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_physics_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddPhysicsVolume(this, RequestId, Payload, Socket) : HandleCreatePhysicsVolume(this, RequestId, Payload, Socket); }
    if (SubAction == TEXT("add_cull_distance_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddCullDistanceVolume(this, RequestId, Payload, Socket) : HandleCreateCullDistanceVolume(this, RequestId, Payload, Socket); }
#if MCP_HAS_POSTPROCESS_VOLUME
    if (SubAction == TEXT("add_post_process_volume")) { return Payload->HasField(TEXT("actorPath")) ? HandleAddPostProcessVolume(this, RequestId, Payload, Socket) : HandleCreatePostProcessVolume(this, RequestId, Payload, Socket); }
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
}
