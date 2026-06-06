#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

namespace McpPerformanceHandlers
{
FString ResolvePerformanceAction(
    const FString& RequestAction,
    const TSharedPtr<FJsonObject>& Payload)
{
    FString Lower = RequestAction;
    if ((RequestAction == TEXT("manage_performance") ||
         RequestAction == TEXT("system_control")) &&
        Payload.IsValid())
    {
        FString SubAction;
        Payload->TryGetStringField(TEXT("subAction"), SubAction);
        if (SubAction.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("action"), SubAction);
        }
        Lower = SubAction.ToLower();
        Lower.ReplaceInline(TEXT("-"), TEXT("_"));
        Lower.ReplaceInline(TEXT(" "), TEXT("_"));
    }
    return Lower;
}

bool IsPerformanceAction(const FString& RequestAction, const FString& Lower)
{
    return RequestAction == TEXT("manage_performance") ||
           RequestAction == TEXT("system_control") ||
           Lower.StartsWith(TEXT("generate_memory_report")) ||
           Lower.StartsWith(TEXT("configure_texture_streaming")) ||
           Lower.StartsWith(TEXT("merge_actors")) ||
           Lower.StartsWith(TEXT("start_profiling")) ||
           Lower.StartsWith(TEXT("stop_profiling")) ||
           Lower.StartsWith(TEXT("show_fps")) ||
           Lower.StartsWith(TEXT("show_stats")) ||
           Lower.StartsWith(TEXT("set_scalability")) ||
           Lower.StartsWith(TEXT("set_resolution_scale")) ||
           Lower.StartsWith(TEXT("set_vsync")) ||
           Lower.StartsWith(TEXT("set_frame_rate_limit")) ||
           Lower.StartsWith(TEXT("configure_nanite")) ||
           Lower.StartsWith(TEXT("configure_lod")) ||
           Lower.StartsWith(TEXT("run_benchmark")) ||
           Lower.StartsWith(TEXT("enable_gpu_timing")) ||
           Lower.StartsWith(TEXT("apply_baseline_settings")) ||
           Lower.StartsWith(TEXT("optimize_draw_calls")) ||
           Lower.StartsWith(TEXT("configure_occlusion_culling")) ||
           Lower.StartsWith(TEXT("optimize_shaders")) ||
           Lower.StartsWith(TEXT("configure_world_partition"));
}
}

bool UMcpAutomationBridgeSubsystem::HandlePerformanceAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString RequestAction = Action.ToLower();
    const FString Lower =
        McpPerformanceHandlers::ResolvePerformanceAction(RequestAction, Payload);

    if (!McpPerformanceHandlers::IsPerformanceAction(RequestAction, Lower))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Performance payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    if (Lower.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("manage_performance requires action or subAction"),
                            TEXT("INVALID_ACTION"));
        return true;
    }

    const McpPerformanceHandlers::FPerformanceActionContext Context{
        *this,
        RequestId,
        Payload,
        RequestingSocket,
        Lower,
        CurrentRequestOrigin};

    if (McpPerformanceHandlers::HandleProfilingAction(Context) ||
        McpPerformanceHandlers::HandleRenderingSettingsAction(Context) ||
        McpPerformanceHandlers::HandleActorMergeAction(Context) ||
        McpPerformanceHandlers::HandleAdvancedOptimizationAction(Context))
    {
        return true;
    }

    return false;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Performance actions require editor build"),
                           nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
