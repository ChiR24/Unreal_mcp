#include "McpAutomationBridgeSubsystem.h"

#include "MCP/McpConsolidatedActionRouting.h"

#define MCP_REGISTER_DIRECT(ActionName, MethodName) RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { return MethodName(R, A, P, S); })

void UMcpAutomationBridgeSubsystem::RegisterAudioAnimationHandlers()
{
    RegisterHandler(
        TEXT("manage_audio"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsAudioAuthoringAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageAudioAuthoringAction(
                    R,
                    TEXT("manage_audio_authoring"),
                    RoutedPayload,
                    S);
            }
            return HandleAudioAction(R, A, P, S);
        });

    MCP_REGISTER_DIRECT("manage_audio_authoring", HandleManageAudioAuthoringAction);
    MCP_REGISTER_DIRECT("manage_lighting", HandleLightingAction);
    MCP_REGISTER_DIRECT("manage_physics", HandleAnimationPhysicsAction);

    RegisterHandler(
        TEXT("animation_physics"),
        [this](const FString& R, const FString& A, const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S)
        {
            const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(P);
            if (McpConsolidatedActions::IsAnimationAuthoringAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageAnimationAuthoringAction(
                    R,
                    TEXT("manage_animation_authoring"),
                    RoutedPayload,
                    S);
            }
            if (McpConsolidatedActions::IsSkeletonAction(SubAction))
            {
                const TSharedPtr<FJsonObject> RoutedPayload =
                    McpConsolidatedActions::WithPayloadSubAction(P, SubAction);
                return HandleManageSkeleton(R, TEXT("manage_skeleton"), RoutedPayload, S);
            }
            return HandleAnimationPhysicsAction(R, A, P, S);
        });

    MCP_REGISTER_DIRECT("manage_animation_authoring", HandleManageAnimationAuthoringAction);
    MCP_REGISTER_DIRECT("manage_effect", HandleEffectAction);
    MCP_REGISTER_DIRECT("create_effect", HandleEffectAction);
    MCP_REGISTER_DIRECT("clear_debug_shapes", HandleEffectAction);
    MCP_REGISTER_DIRECT("manage_performance", HandlePerformanceAction);
    MCP_REGISTER_DIRECT("enable_gpu_timing", HandlePerformanceAction);
    MCP_REGISTER_DIRECT("manage_debug", HandleDebugAction);
    MCP_REGISTER_DIRECT("spawn_category", HandleDebugAction);
}

#undef MCP_REGISTER_DIRECT
