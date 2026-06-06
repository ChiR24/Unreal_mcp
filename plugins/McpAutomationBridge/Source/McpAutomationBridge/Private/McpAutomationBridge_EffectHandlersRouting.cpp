#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

namespace McpEffectHandlers
{
FString NormalizeNativeSubAction(
    const FString& Lower,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload)
{
    FString SubAction;
    Payload->TryGetStringField(TEXT("subAction"), SubAction);
    if (SubAction.IsEmpty())
    {
        Payload->TryGetStringField(TEXT("action"), SubAction);
    }
    if (SubAction.IsEmpty() && !Lower.Equals(TEXT("manage_effect")) &&
        !Lower.Equals(TEXT("create_effect")))
    {
        SubAction = Action;
    }
    SubAction = SubAction.ToLower();
    SubAction.ReplaceInline(TEXT("-"), TEXT("_"));
    SubAction.ReplaceInline(TEXT(" "), TEXT("_"));
    if (!SubAction.IsEmpty())
    {
        Payload->SetStringField(TEXT("subAction"), SubAction);
    }
    return SubAction;
}

bool IsNiagaraAuthoringSubAction(const FString& SubAction)
{
    static const TSet<FString> NiagaraAuthoringActions = {
        TEXT("create_niagara_system"),
        TEXT("create_niagara_emitter"),
        TEXT("add_emitter_to_system"),
        TEXT("set_emitter_properties"),
        TEXT("add_spawn_rate_module"),
        TEXT("add_spawn_burst_module"),
        TEXT("add_spawn_per_unit_module"),
        TEXT("add_initialize_particle_module"),
        TEXT("add_particle_state_module"),
        TEXT("add_force_module"),
        TEXT("add_velocity_module"),
        TEXT("add_acceleration_module"),
        TEXT("add_size_module"),
        TEXT("add_color_module"),
        TEXT("add_sprite_renderer_module"),
        TEXT("add_mesh_renderer_module"),
        TEXT("add_ribbon_renderer_module"),
        TEXT("add_light_renderer_module"),
        TEXT("add_collision_module"),
        TEXT("add_kill_particles_module"),
        TEXT("add_camera_offset_module"),
        TEXT("add_user_parameter"),
        TEXT("set_parameter_value"),
        TEXT("bind_parameter_to_source"),
        TEXT("add_skeletal_mesh_data_interface"),
        TEXT("add_static_mesh_data_interface"),
        TEXT("add_spline_data_interface"),
        TEXT("add_audio_spectrum_data_interface"),
        TEXT("add_collision_query_data_interface"),
        TEXT("add_event_generator"),
        TEXT("add_event_receiver"),
        TEXT("configure_event_payload"),
        TEXT("enable_gpu_simulation"),
        TEXT("add_simulation_stage"),
        TEXT("get_niagara_info"),
        TEXT("validate_niagara_system")};
    return NiagaraAuthoringActions.Contains(SubAction);
}

bool IsNiagaraGraphSubAction(const FString& SubAction)
{
    return SubAction == TEXT("add_niagara_module") ||
           SubAction == TEXT("connect_niagara_pins") ||
           SubAction == TEXT("remove_niagara_node") ||
           SubAction == TEXT("add_module") ||
           SubAction == TEXT("connect_pins") ||
           SubAction == TEXT("remove_node");
}

FString ResolveCreateEffectSubAction(
    const FString& Action,
    const FString& Lower,
    const TSharedPtr<FJsonObject>& Payload)
{
    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    if (Lower.Equals(TEXT("create_niagara_system")))
    {
        SubAction = TEXT("create_niagara_system");
    }
    if (SubAction.IsEmpty() &&
        !Action.Equals(TEXT("create_effect"), ESearchCase::IgnoreCase))
    {
        SubAction = Action;
    }
    return SubAction.ToLower();
}

bool HandleCreateEffectSubAction(
    const FEffectActionContext& Context,
    const FString& LowerSubAction)
{
    if (LowerSubAction == TEXT("debug_shape"))
    {
        return HandleDrawDebugShape(Context);
    }
    if (LowerSubAction == TEXT("particle"))
    {
        return HandleParticleDebugShape(Context);
    }
    if (LowerSubAction == TEXT("niagara") ||
        LowerSubAction == TEXT("spawn_niagara"))
    {
        return false;
    }
    if (LowerSubAction == TEXT("set_niagara_parameter"))
    {
        return HandleSetNiagaraParameter(Context);
    }
    if (LowerSubAction == TEXT("activate_niagara") ||
        LowerSubAction == TEXT("deactivate_niagara") ||
        LowerSubAction == TEXT("advance_simulation"))
    {
        return HandleNiagaraLifecycleAction(Context, LowerSubAction);
    }
    if (LowerSubAction == TEXT("create_dynamic_light"))
    {
        return HandleCreateDynamicLight(Context);
    }
    if (LowerSubAction == TEXT("cleanup"))
    {
        return HandleCleanup(Context, true);
    }
    return false;
}
}
