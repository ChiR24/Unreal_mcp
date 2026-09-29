#include "McpAutomationBridgeSubsystem.h"

#include "MCP/Routing/McpConsolidatedActionRouting.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#include <initializer_list>

namespace
{
using FHandlerMethod = bool (UMcpAutomationBridgeSubsystem::*)(
    const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

// One sibling domain inside a parent tool: the sub-actions Claims accepts go to
// Handler under the domain's own Action name (nullptr keeps the parent's).
struct FSubRoute
{
    bool (*Claims)(const FString&);
    FHandlerMethod Handler;
    const TCHAR* Domain;
};

bool IsBlueprintGraphAction(const FString& SubAction)
{
    static const TSet<FString> Actions = {
        TEXT("create_node"), TEXT("delete_node"), TEXT("connect_pins"),
        TEXT("break_pin_links"), TEXT("set_node_property"),
        TEXT("create_reroute_node"), TEXT("get_node_details"),
        TEXT("get_graph_details"), TEXT("get_pin_details"),
        TEXT("list_node_types"), TEXT("set_pin_default_value"),
        TEXT("list_animbp_graphs"), TEXT("get_transition_rule_graph"),
        TEXT("build_graph")};
    return Actions.Contains(SubAction);
}

// connect_material_pins spells its endpoints several ways; the material
// authoring handler reads only the canonical names.
void NormalizeMaterialConnectionAliases(const TSharedPtr<FJsonObject>& Payload)
{
    const auto Alias = [&Payload](const TCHAR* Canonical, std::initializer_list<const TCHAR*> Aliases)
    {
        const FString Value = McpGetFirstStringField(Payload, Aliases);
        if (McpGetFirstStringField(Payload, {Canonical}).IsEmpty() && !Value.IsEmpty())
        {
            Payload->SetStringField(Canonical, Value);
        }
    };
    Alias(TEXT("sourceNodeId"), {TEXT("fromNodeId"), TEXT("fromNode"), TEXT("sourceNode")});
    Alias(TEXT("targetNodeId"), {TEXT("toNodeId"), TEXT("toNode"), TEXT("targetNode")});
    Alias(TEXT("sourcePin"), {TEXT("fromPin"), TEXT("outputPin"), TEXT("sourceOutputPin")});
    Alias(TEXT("inputName"), {TEXT("targetPin"), TEXT("toPin"), TEXT("inputPin")});
}

// Action names are lower_snake_case identifiers; anything else is a typo in a
// registration table, so refuse it loudly instead of registering it.
bool IsValidActionIdentifier(const FString& Action)
{
    // ASCII ranges, not FChar::IsLower/IsDigit: those accept 'é' and other Unicode letters.
    auto Lower = [](TCHAR C) { return C >= TCHAR('a') && C <= TCHAR('z'); };
    if (Action.IsEmpty() || Action.Len() > 96 || !Lower(Action[0]))
    {
        return false;
    }
    for (const TCHAR Character : Action)
    {
        if (!Lower(Character) && !(Character >= TCHAR('0') && Character <= TCHAR('9')) && Character != TCHAR('_'))
        {
            return false;
        }
    }
    return true;
}
}

// The wire contract is the canonical parent tools plus console_command. Each
// parent reads its sub-action from the payload and routes it to the domain
// handler that owns it.
void UMcpAutomationBridgeSubsystem::InitializeHandlers()
{
    using namespace McpConsolidatedActions;
    using S = UMcpAutomationBridgeSubsystem;

    const auto Route = [this](const TCHAR* Parent, FHandlerMethod Fallback, TArray<FSubRoute> SubRoutes)
    {
        RegisterHandler(Parent, [this, Fallback, SubRoutes = MoveTemp(SubRoutes)](
            const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> Socket)
        {
            const FString SubAction = GetPayloadSubAction(P);
            for (const FSubRoute& Sub : SubRoutes)
            {
                if (Sub.Claims(SubAction))
                {
                    return (this->*Sub.Handler)(R, Sub.Domain ? FString(Sub.Domain) : A, WithPayloadSubAction(P, SubAction), Socket);
                }
            }
            return (this->*Fallback)(R, A, P, Socket);
        });
    };

    const struct
    {
        const TCHAR* Name;
        FHandlerMethod Handler;
    } Directs[] = {
        {TEXT("console_command"), &S::HandleConsoleCommandAction},
        {TEXT("control_actor"), &S::HandleControlActorAction},
        {TEXT("control_editor"), &S::HandleControlEditorAction},
        {TEXT("inspect"), &S::HandleInspectAction},
        {TEXT("manage_ai"), &S::HandleManageAIAction},
        {TEXT("manage_character"), &S::HandleManageCharacterAction},
        {TEXT("manage_combat"), &S::HandleManageCombatAction},
        {TEXT("manage_effect"), &S::HandleEffectAction},
        {TEXT("manage_gas"), &S::HandleManageGASAction},
        {TEXT("manage_geometry"), &S::HandleGeometryAction},
        {TEXT("manage_interaction"), &S::HandleManageInteractionAction},
        {TEXT("manage_inventory"), &S::HandleManageInventoryAction},
        {TEXT("manage_level"), &S::HandleLevelAction},
        {TEXT("manage_pcg"), &S::HandleManagePCGAction},
        {TEXT("manage_sequence"), &S::HandleSequenceAction}};
    for (const auto& Direct : Directs)
    {
        Route(Direct.Name, Direct.Handler, {});
    }

    Route(TEXT("animation_physics"), &S::HandleAnimationPhysicsAction, {
        {&IsAnimationAuthoringAction, &S::HandleManageAnimationAuthoringAction, TEXT("manage_animation_authoring")},
        {&IsSkeletonAction, &S::HandleManageSkeleton, TEXT("manage_skeleton")}});
    Route(TEXT("build_environment"), &S::HandleBuildEnvironmentAction, {
        {&IsLightingAction, &S::HandleLightingAction, TEXT("manage_lighting")},
        {&IsSplineAction, &S::HandleManageSplinesAction, TEXT("manage_splines")},
        {&IsRenderingAction, &S::HandleRenderAction, TEXT("manage_render")}});
    Route(TEXT("manage_audio"), &S::HandleAudioAction, {
        {&IsAudioAuthoringAction, &S::HandleManageAudioAuthoringAction, TEXT("manage_audio_authoring")}});
    Route(TEXT("manage_blueprint"), &S::HandleBlueprintAction, {
        {&IsWidgetAuthoringAction, &S::HandleManageWidgetAuthoringAction, TEXT("manage_widget_authoring")},
        {&IsBlueprintGraphAction, &S::HandleBlueprintGraphAction, nullptr}});
    Route(TEXT("manage_level_structure"), &S::HandleManageLevelStructureAction, {
        {&IsVolumeAction, &S::HandleManageVolumesAction, TEXT("manage_volumes")}});
    Route(TEXT("manage_networking"), &S::HandleManageNetworkingAction, {
        {&IsInputAction, &S::HandleInputAction, TEXT("manage_input")},
        {&IsGameFrameworkAction, &S::HandleManageGameFrameworkAction, TEXT("manage_game_framework")},
        {&IsSessionAction, &S::HandleManageSessionsAction, TEXT("manage_sessions")}});
    Route(TEXT("system_control"), &S::HandleSystemControlAction, {
        {&IsPerformanceAction, &S::HandlePerformanceAction, TEXT("manage_performance")},
        {&IsSystemUiAction, &S::HandleUiAction, nullptr}});

    // manage_asset renames three material sub-actions to the authoring
    // handler's own verbs before routing them.
    RegisterHandler(TEXT("manage_asset"), [this](
        const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> Socket)
    {
        FString SubAction = GetPayloadSubAction(P);
        if (Texture().Contains(SubAction))
        {
            return HandleManageTextureAction(R, TEXT("manage_texture"), WithPayloadSubAction(P, SubAction), Socket);
        }
        if (!MaterialAuthoring().Contains(SubAction))
        {
            return HandleAssetAction(R, A, P, Socket);
        }
        static const TMap<FString, FString> Renamed = {
            {TEXT("connect_material_pins"), TEXT("connect_nodes")},
            {TEXT("break_material_connections"), TEXT("disconnect_nodes")},
            {TEXT("rebuild_material"), TEXT("compile_material")}};
        if (const FString* Verb = Renamed.Find(SubAction))
        {
            SubAction = *Verb;
        }
        const TSharedPtr<FJsonObject> Routed = WithPayloadSubAction(P, SubAction);
        if (SubAction == TEXT("connect_nodes") && Routed.IsValid())
        {
            NormalizeMaterialConnectionAliases(Routed);
        }
        return HandleManageMaterialAuthoringAction(R, TEXT("manage_material_authoring"), Routed, Socket);
    });
}

bool UMcpAutomationBridgeSubsystem::RegisterHandler(
    const FString& Action,
    FAutomationHandler Handler)
{
    if (!IsValidActionIdentifier(Action) || !Handler || AutomationHandlers.Contains(Action))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
            TEXT("Skipping automation handler '%s': invalid name, empty callback, or duplicate."), *Action);
        return false;
    }
    AutomationHandlers.Add(Action, MoveTemp(Handler));
    return true;
}
