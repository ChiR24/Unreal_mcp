#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

namespace McpInsights
{
bool HandleStopSession(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("stop_session"), TEXT("stop_trace"));
    const bool bStopped = FTraceAuxiliary::Stop();
    Result->SetStringField(TEXT("status"), bStopped ? TEXT("stopped") : TEXT("not_active"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        bStopped ? TEXT("Trace session stopped.") : TEXT("Trace session was not active."),
        Result);
    return true;
}

bool HandlePauseSession(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("pause_session"), TEXT("pause_trace"));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    if (!HasActiveTrace())
    {
        Result->SetStringField(TEXT("status"), TEXT("not_active"));
        AddTraceStatus(Result);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Trace session was not active."), Result, TEXT("TRACE_PAUSE_FAILED"));
        return true;
    }
    const bool bWasPaused = FTraceAuxiliary::IsPaused();
    FTraceAuxiliary::Pause();
    const bool bPaused = FTraceAuxiliary::IsPaused();
    Result->SetStringField(TEXT("status"),
        bPaused ? (bWasPaused ? TEXT("already_paused") : TEXT("paused")) :
                  TEXT("not_active"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, bPaused,
        bPaused ? TEXT("Trace session paused.") :
                  TEXT("Trace session was not active."),
        Result, bPaused ? FString() : TEXT("TRACE_PAUSE_FAILED"));
#else
    const bool bPauseRequested = FTraceAuxiliary::Pause();
    Result->SetStringField(TEXT("status"), TEXT("pause_requested"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, bPauseRequested,
        TEXT("Trace pause requested."), Result,
        bPauseRequested ? FString() : TEXT("TRACE_PAUSE_FAILED"));
#endif
    return true;
}

bool HandleResumeSession(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("resume_session"), TEXT("resume_trace"));
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    if (!HasActiveTrace())
    {
        Result->SetStringField(TEXT("status"), TEXT("not_active"));
        AddTraceStatus(Result);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Trace session was not active."), Result, TEXT("TRACE_RESUME_FAILED"));
        return true;
    }
    const bool bWasPaused = FTraceAuxiliary::IsPaused();
    if (bWasPaused)
    {
        FTraceAuxiliary::Resume();
    }
    const bool bResumed = bWasPaused && !FTraceAuxiliary::IsPaused();
    Result->SetStringField(TEXT("status"), bResumed ? TEXT("resumed") : TEXT("not_paused"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, bResumed,
        bResumed ? TEXT("Trace session resumed.") : TEXT("Trace session was not paused."),
        Result, bResumed ? FString() : TEXT("TRACE_RESUME_FAILED"));
#else
    const bool bResumeRequested = FTraceAuxiliary::Resume();
    Result->SetStringField(TEXT("status"), TEXT("resume_requested"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, bResumeRequested,
        TEXT("Trace resume requested."), Result,
        bResumeRequested ? FString() : TEXT("TRACE_RESUME_FAILED"));
#endif
    return true;
}

bool HandleGetTraceStatus(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("get_trace_status"));
    Result->SetStringField(TEXT("status"), TEXT("queried"));
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Trace status queried."), Result);
    return true;
}
}
