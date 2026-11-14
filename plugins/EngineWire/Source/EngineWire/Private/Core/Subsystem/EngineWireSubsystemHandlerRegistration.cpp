#include "EngineWireSubsystem.h"

#include "Core/Routing/McpConsolidatedActionRouting.h"

namespace
{
using FHandlerMethod = bool (UEngineWireSubsystem::*)(
    const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

// One sibling domain inside a parent tool: the sub-actions Claims accepts go to
// Handler under the domain's own Action name (nullptr keeps the parent's).
struct FSubRoute
{
    bool (*Claims)(const FString&);
    FHandlerMethod Handler;
    const TCHAR* Domain;
};

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
void UEngineWireSubsystem::InitializeHandlers()
{
    using namespace McpConsolidatedActions;
    using S = UEngineWireSubsystem;

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
    // The material dispatcher renames its old verbs (connect_material_pins, rebuild_material...) itself.
    Route(TEXT("manage_asset"), &S::HandleAssetAction, {
        {&IsTextureAction, &S::HandleManageTextureAction, TEXT("manage_texture")},
        {&IsMaterialAuthoringAction, &S::HandleManageMaterialAuthoringAction, TEXT("manage_material_authoring")}});
}

bool UEngineWireSubsystem::RegisterHandler(
    const FString& Action,
    FAutomationHandler Handler)
{
    if (!IsValidActionIdentifier(Action) || !Handler || AutomationHandlers.Contains(Action))
    {
        UE_LOG(LogEngineWireSubsystem, Warning,
            TEXT("Skipping automation handler '%s': invalid name, empty callback, or duplicate."), *Action);
        return false;
    }
    AutomationHandlers.Add(Action, MoveTemp(Handler));
    return true;
}
