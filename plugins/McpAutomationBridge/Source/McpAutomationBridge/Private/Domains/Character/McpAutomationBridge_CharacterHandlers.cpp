#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"
#include "Domains/MetaHuman/McpAutomationBridge_MetaHumanHandlers.h"

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
    if (SubAction == TEXT("configure_nav_movement")) return HandleConfigureNavMovement(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("get_character_info")) return HandleGetCharacterInfo(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("setup_movement")) return HandleSetupMovement(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_walk_speed")) return HandleSetWalkSpeed(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_jump_height")) return HandleSetJumpHeight(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_gravity_scale")) return HandleSetGravityScale(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_ground_friction")) return HandleSetGroundFriction(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("set_braking_deceleration")) return HandleSetBrakingDeceleration(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("configure_crouch")) return HandleConfigureCrouch(this, RequestId, Payload, RequestingSocket);

    // MetaHuman Creator (UE 5.6+). Implemented reflectively in Domains/MetaHuman/
    // so the plugin still builds on engines that ship no MetaHuman at all.
    if (SubAction == TEXT("metahuman_status")) return McpMetaHumanHandlers::HandleMetaHumanStatus(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("create_metahuman")) return McpMetaHumanHandlers::HandleCreateMetaHuman(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("rig_metahuman")) return McpMetaHumanHandlers::HandleRigMetaHuman(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("build_metahuman")) return McpMetaHumanHandlers::HandleBuildMetaHuman(this, RequestId, Payload, RequestingSocket);
    if (SubAction == TEXT("export_metahuman")) return McpMetaHumanHandlers::HandleExportMetaHuman(this, RequestId, Payload, RequestingSocket);

    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown character subAction: %s"), *SubAction), TEXT("UNKNOWN_SUBACTION"));
    return true;
}
