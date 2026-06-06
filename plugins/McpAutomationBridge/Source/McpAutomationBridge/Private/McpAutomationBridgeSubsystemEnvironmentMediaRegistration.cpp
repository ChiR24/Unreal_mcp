#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterEnvironmentMediaHandlers()
{
    MCP_REGISTER_DIRECT("create_niagara_system", HandleCreateNiagaraSystem);
    MCP_REGISTER_DIRECT("create_niagara_ribbon", HandleCreateNiagaraRibbon);
    MCP_REGISTER_DIRECT("create_niagara_emitter", HandleCreateNiagaraEmitter);
    MCP_REGISTER_DIRECT("spawn_niagara_actor", HandleSpawnNiagaraActor);
    MCP_REGISTER_DIRECT("modify_niagara_parameter", HandleModifyNiagaraParameter);
    MCP_REGISTER_DIRECT("manage_niagara_authoring", HandleManageNiagaraAuthoringAction);
    MCP_REGISTER_DIRECT("create_anim_blueprint", HandleCreateAnimBlueprint);
    MCP_REGISTER_DIRECT("play_anim_montage", HandlePlayAnimMontage);
    MCP_REGISTER_DIRECT("setup_ragdoll", HandleSetupRagdoll);
    MCP_REGISTER_DIRECT("activate_ragdoll", HandleActivateRagdoll);
    MCP_REGISTER_DIRECT("add_material_texture_sample", HandleAddMaterialTextureSample);
    MCP_REGISTER_DIRECT("add_material_expression", HandleAddMaterialExpression);
    MCP_REGISTER_DIRECT("create_material_nodes", HandleCreateMaterialNodes);
    MCP_REGISTER_DIRECT("add_sequencer_keyframe", HandleAddSequencerKeyframe);
    MCP_REGISTER_DIRECT("manage_sequencer_track", HandleManageSequencerTrack);
    MCP_REGISTER_DIRECT("add_camera_track", HandleAddCameraTrack);
    MCP_REGISTER_DIRECT("add_animation_track", HandleAddAnimationTrack);
    MCP_REGISTER_DIRECT("add_transform_track", HandleAddTransformTrack);
    MCP_REGISTER_DIRECT("manage_ui", HandleUiAction);
    MCP_REGISTER_DIRECT("control_environment", HandleControlEnvironmentAction);

    RegisterHandler(
        TEXT("build_environment"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsLightingAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleLightingAction(R, TEXT("manage_lighting"), RoutedPayload, S);
            }
            if (McpConsolidatedActions::IsSplineAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageSplinesAction(R, TEXT("manage_splines"), RoutedPayload, S);
            }
            return HandleBuildEnvironmentAction(R, A, P, S);
        });
}

#undef MCP_REGISTER_DIRECT
