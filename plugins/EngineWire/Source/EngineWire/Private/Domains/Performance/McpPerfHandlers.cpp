#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Domains/Performance/Budget/McpFrameBudget.h"
#include "Domains/Performance/McpPerfPrivate.h"

bool UEngineWireSubsystem::HandlePerformanceAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Reached only through system_control's sub-route, which claimed the sub-action and wrote it into the payload.
    const FString Lower = McpConsolidatedActions::GetPayloadSubAction(Payload);

    if (Lower == TEXT("fit_frame_budget"))
    {
        return McpFrameBudget::Handle(this, RequestId, Payload, RequestingSocket);
    }

    const McpPerformanceHandlers::FPerformanceActionContext Context{
        *this,
        RequestId,
        Payload,
        RequestingSocket,
        Lower,
        CurrentRequestOrigin};

    return McpPerformanceHandlers::HandleProfilingAction(Context) ||
           McpPerformanceHandlers::HandleRenderingSettingsAction(Context) ||
           McpPerformanceHandlers::HandleActorMergeAction(Context) ||
           McpPerformanceHandlers::HandleAdvancedOptimizationAction(Context);
}
