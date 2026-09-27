#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleInsightsAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Reached only through system_control's manage_insights route, with a valid payload.
    const FString SubAction = McpInsights::NormalizeSubAction(Payload);
    if (SubAction == TEXT("start_session"))
    {
        return McpInsights::HandleStartSession(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("stop_session"))
    {
        return McpInsights::HandleStopSession(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("pause_session"))
    {
        return McpInsights::HandlePauseSession(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("resume_session"))
    {
        return McpInsights::HandleResumeSession(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("get_trace_status"))
    {
        return McpInsights::HandleGetTraceStatus(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("write_snapshot"))
    {
        return McpInsights::HandleWriteSnapshot(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("send_snapshot"))
    {
        return McpInsights::HandleSendSnapshot(this, RequestId, Payload,
            RequestingSocket);
    }
    if (SubAction == TEXT("analyze_trace"))
    {
        return McpInsights::HandleAnalyzeTrace(this, RequestId, Payload,
            RequestingSocket);
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."),
        TEXT("INVALID_SUBACTION"));
    return true;
}
