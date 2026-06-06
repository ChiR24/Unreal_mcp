#include "McpAutomationBridge_CharacterHandlers.h"

DEFINE_LOG_CATEGORY(LogMcpCharacterHandlers);

bool UMcpAutomationBridgeSubsystem::HandleManageCharacterAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_character"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Character handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'subAction' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    using namespace McpCharacterHandlers;
    if (SubAction == TEXT("create_character_blueprint")) return HandleCreateCharacterBlueprint(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_capsule_component")) return HandleConfigureCapsuleComponent(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_mesh_component")) return HandleConfigureMeshComponent(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_camera_component")) return HandleConfigureCameraComponent(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_movement_speeds")) return HandleConfigureMovementSpeeds(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_jump")) return HandleConfigureJump(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_rotation")) return HandleConfigureRotation(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("add_custom_movement_mode")) return HandleAddCustomMovementMode(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_nav_movement")) return HandleConfigureNavMovement(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_mantling")) return HandleSetupMantling(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_vaulting")) return HandleSetupVaulting(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_climbing")) return HandleSetupClimbing(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_sliding")) return HandleSetupSliding(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_wall_running")) return HandleSetupWallRunning(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_grappling")) return HandleSetupGrappling(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_footstep_system")) return HandleSetupFootstepSystem(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("map_surface_to_sound")) return HandleMapSurfaceToSound(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_footstep_fx")) return HandleConfigureFootstepFx(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("get_character_info")) return HandleGetCharacterInfo(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_movement")) return HandleSetupMovement(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_walk_speed")) return HandleSetWalkSpeed(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_jump_height")) return HandleSetJumpHeight(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_gravity_scale")) return HandleSetGravityScale(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_ground_friction")) return HandleSetGroundFriction(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_braking_deceleration")) return HandleSetBrakingDeceleration(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_crouch")) return HandleConfigureCrouch(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_sprint")) return HandleConfigureSprint(this, RequestId, Payload, RequestingSocket);

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}
