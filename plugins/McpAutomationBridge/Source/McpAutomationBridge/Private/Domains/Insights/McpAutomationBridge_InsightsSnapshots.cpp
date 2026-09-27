#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Insights/McpAutomationBridge_InsightsRequests.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

namespace McpInsights
{
bool HandleWriteSnapshot(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    FString Path;
    FString Error;
    FString ErrorCode;
    if (!TryResolveTracePath(Payload, false, true, true, Path, Error, ErrorCode))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }

    const bool bWritten = FTraceAuxiliary::WriteSnapshot(*Path);
    if (!bWritten)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to write trace snapshot."), TEXT("SNAPSHOT_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("write_snapshot"));
    Result->SetStringField(TEXT("status"), TEXT("snapshot_written"));
    Result->SetStringField(TEXT("snapshotPath"), Path);
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Trace snapshot written."), Result);
#else
    Bridge->SendAutomationError(RequestingSocket, RequestId,
        TEXT("Trace snapshots require Unreal Engine 5.3 or newer."),
        TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool HandleSendSnapshot(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    FString Host;
    int32 Port = 0;
    FString Error;
    FString ErrorCode;
    if (!TryReadHostAndPort(Payload, false, Host, Port, Error, ErrorCode))
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }

    const bool bSent = FTraceAuxiliary::SendSnapshot(*Host, Port);
    if (!bSent)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to send trace snapshot."), TEXT("SNAPSHOT_SEND_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result =
        CreateInsightsResult(TEXT("send_snapshot"));
    Result->SetStringField(TEXT("status"), TEXT("snapshot_sent"));
    Result->SetStringField(TEXT("host"), Host);
    Result->SetNumberField(TEXT("port"), Port);
    AddTraceStatus(Result);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Trace snapshot sent."), Result);
#else
    Bridge->SendAutomationError(RequestingSocket, RequestId,
        TEXT("Trace snapshots require Unreal Engine 5.3 or newer."),
        TEXT("NOT_SUPPORTED"));
#endif
    return true;
}
}
