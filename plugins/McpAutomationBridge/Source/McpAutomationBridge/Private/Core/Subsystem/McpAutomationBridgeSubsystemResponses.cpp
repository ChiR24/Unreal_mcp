#include "McpAutomationBridgeSubsystem.h"

#include "MCP/Transport/McpNativeTransport.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace McpAutomationBridgeSubsystemResponse;

namespace
{
bool IsLogAutomationEvent(const TSharedPtr<FJsonObject>& Event)
{
    FString EventName;
    return Event.IsValid() &&
        Event->TryGetStringField(TEXT("event"), EventName) &&
        EventName.Equals(TEXT("log"), ESearchCase::CaseSensitive);
}
}

void UMcpAutomationBridgeSubsystem::BroadcastAutomationEvent(
    const TSharedPtr<FJsonObject>& Event,
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket)
{
    if (!Event.IsValid())
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Automation event broadcast skipped because the event object was invalid"));
        return;
    }

    FString SerializedEvent;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&SerializedEvent);
    if (!FJsonSerializer::Serialize(Event.ToSharedRef(), Writer))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Automation event broadcast skipped because serialization failed"));
        return;
    }

    const bool bLogEvent = IsLogAutomationEvent(Event);
    if (ConnectionManager.IsValid())
    {
        if (bLogEvent)
        {
            ConnectionManager->SendRawMessageToLogSubscribers(SerializedEvent);
        }
        else if (TargetSocket.IsValid())
        {
            ConnectionManager->SendRawMessageToSocket(TargetSocket, SerializedEvent);
        }
    }

    if (bLogEvent && NativeTransport)
    {
        NativeTransport->BroadcastLogEventNotification(Event);
    }
}

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    const bool bSuccess,
    const FString& Message,
    const TSharedPtr<FJsonObject>& Result,
    const FString& ErrorCode,
    ERequestOrigin Origin)
{
    ClearAutomationRequestCancellation(RequestId);

    bool bEffectiveSuccess = bSuccess;
    FString EffectiveMessage = Message;
    FString EffectiveErrorCode = ErrorCode;
    TSharedPtr<FJsonObject> EffectiveResult = Result;

    if (bSuccess && bProcessingAutomationRequest)
    {
        TArray<FString> CapturedErrors;
        int32 TotalCapturedErrorCount = 0;
        bool bCapturedErrorsTruncated = false;
        {
            FScopeLock Lock(&ErrorCaptureMutex);
            if (CurrentErrorCapture.bHasErrors.load())
            {
                CapturedErrors = CurrentErrorCapture.ErrorMessages;
                TotalCapturedErrorCount = CurrentErrorCapture.ErrorCount;
                bCapturedErrorsTruncated = CurrentErrorCapture.bErrorMessagesTruncated;
            }
        }

        if (CapturedErrors.Num() > 0)
        {
            // Surface, don't override: engine-log errors observed during the
            // request are ATTACHED for the caller to judge, but the handler's
            // own verdict stands. Downgrading success here conflated transport
            // success with asset-level warnings and produced false negatives —
            // a handler that completed its work (node created, asset saved)
            // was reported as failed, triggering pointless retries and
            // undo-then-reapply flows. Handlers that can genuinely fail are
            // responsible for reporting it themselves (e.g. blueprint_compile
            // returns its real compile status).
            TSharedPtr<FJsonObject> AugmentedResult = MakeShared<FJsonObject>();
            if (Result.IsValid())
            {
                for (const auto& Pair : Result->Values)
                {
                    AugmentedResult->SetField(Pair.Key, Pair.Value);
                }
            }

            TArray<TSharedPtr<FJsonValue>> ErrorValues;
            const int32 MaxErrorsInResponse = 3;
            const int32 ErrorResponseCount =
                FMath::Min(CapturedErrors.Num(), MaxErrorsInResponse);
            for (int32 ErrorIndex = 0; ErrorIndex < ErrorResponseCount; ++ErrorIndex)
            {
                ErrorValues.Add(MakeShared<FJsonValueString>(
                    SanitizeEngineErrorForResponse(CapturedErrors[ErrorIndex])));
            }
            AugmentedResult->SetBoolField(TEXT("engineErrorsObserved"), true);
            AugmentedResult->SetNumberField(
                TEXT("engineErrorCount"),
                TotalCapturedErrorCount);
            AugmentedResult->SetArrayField(TEXT("engineErrors"), ErrorValues);
            if (bCapturedErrorsTruncated || CapturedErrors.Num() > MaxErrorsInResponse)
            {
                AugmentedResult->SetBoolField(TEXT("engineErrorsTruncated"), true);
            }
            EffectiveResult = AugmentedResult;
        }
    }

    if (!bEffectiveSuccess)
    {
        EffectiveMessage = SanitizeEngineErrorForResponse(EffectiveMessage);
    }

    // The registry wins over CurrentRequestOrigin because it is the only source
    // still true for a DEFERRED reply: ProcessAutomationRequest's ON_SCOPE_EXIT
    // resets the global to WebSocket, so a handler answering from an
    // AsyncTask/timer/delegate reached this line with it already cleared, and
    // every native /mcp response from such a handler went down the WebSocket
    // path, was dropped, and hung the caller until the 300s SSE sweeper.
    ERequestOrigin EffectiveOrigin =
        Origin == ERequestOrigin::WebSocket ? CurrentRequestOrigin : Origin;
    ERequestOrigin RecordedOrigin = ERequestOrigin::WebSocket;
    if (FMcpRequestOriginRegistry::Get().Resolve(RequestId, RecordedOrigin))
    {
        EffectiveOrigin = RecordedOrigin;
    }
    // Released here: the single funnel every delivered response passes through.
    FMcpRequestOriginRegistry::Get().Forget(RequestId);
    // BB-005 bounded terminal in the single response funnel, before the
    // transport branch. Persist inline ONLY on the game thread (deferred
    // replies coalesce to the next game-thread persist); the native branch
    // below RETURNS, so this must precede it.
    FMcpDiagnosticsSnapshot::Get().RecordTerminal(RequestId, bEffectiveSuccess ? TEXT("success") : EffectiveErrorCode.IsEmpty() ? TEXT("failure") : EffectiveErrorCode);
    if (IsInGameThread()) { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); }
    // F3 fix: removed the response-stealing override that redirected a
    // WebSocket-originated response to the Native HTTP transport when the
    // RequestId matched a Native pending request. RequestIds are
    // server-generated GUIDs, so the match should never happen in practice,
    // but if a collision ever occurred, the response would leak to the
    // wrong transport. The response now goes to the originator (Origin
    // parameter, possibly resolved via the CurrentRequestOrigin global
    // for WebSocket-originated calls).
    if (EffectiveOrigin == ERequestOrigin::NativeHTTP && NativeTransport)
    {
        // This branch RETURNS, so it never reaches the connection manager that
        // closes the interval for a WebSocket reply. Without this call the
        // interval opened at queue admission is never closed and a native-only
        // deployment scrapes permanently empty counters. Recorded BEFORE the
        // delivery attempt so a dropped delivery is still counted, and only the
        // bounded error CODE is forwarded - EffectiveMessage routinely carries
        // asset paths and object names.
        FMcpTelemetryRegistry::Get().EndRequest(
            RequestId,
            bEffectiveSuccess ? TEXT("success") : TEXT("failure"),
            EffectiveErrorCode);
        if (!NativeTransport->CompletePendingRequest(
                RequestId,
                bEffectiveSuccess,
                EffectiveMessage,
                EffectiveResult,
                EffectiveErrorCode))
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Native HTTP response for %s dropped — request already expired or unknown"),
                *RequestId);
        }
        return;
    }
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendAutomationResponse(
            TargetSocket,
            RequestId,
            bEffectiveSuccess,
            EffectiveMessage,
            EffectiveResult,
            EffectiveErrorCode);
    }
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    const FString& Message,
    const FString& ErrorCode)
{
    const FString ResolvedError =
        ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Warning,
        TEXT("Automation request failed (%s): %s"),
        *ResolvedError,
        *SanitizeForLog(Message));
    SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::SendAutomationRejection(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    EAutomationQueueRejection Reason)
{
    const TCHAR* Code = TEXT("AUTOMATION_REQUEST_REJECTED");
    FString Message = TEXT("Automation request rejected");
    switch (Reason)
    {
        case EAutomationQueueRejection::NotAccepting:
            Code = TEXT("AUTOMATION_NOT_ACCEPTING");
            Message = TEXT("Automation request rejected: subsystem is not accepting requests");
            break;
        case EAutomationQueueRejection::AlreadyCanceled:
            Code = TEXT("AUTOMATION_ALREADY_CANCELED");
            Message = TEXT("Automation request rejected: request was already canceled");
            break;
        case EAutomationQueueRejection::QueueFull:
            Code = TEXT("AUTOMATION_QUEUE_FULL");
            Message = TEXT("Automation request rejected: queue is full");
            break;
        case EAutomationQueueRejection::SessionQueueFull:
            Code = TEXT("AUTOMATION_SESSION_QUEUE_FULL");
            Message = TEXT("Automation request rejected: this session already has the maximum number of queued requests; retry after your queued work drains");
            break;
        default:
            break;
    }
    SendAutomationError(TargetSocket, RequestId, Message, Code);
}

void UMcpAutomationBridgeSubsystem::SendProgressUpdate(
    const FString& RequestId,
    float Percent,
    const FString& Message,
    bool bStillWorking,
    ERequestOrigin Origin)
{
    if (Origin == ERequestOrigin::NativeHTTP && NativeTransport)
    {
        NativeTransport->SendSSEProgressUpdate(RequestId, Percent, Message);
        return;
    }
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendProgressUpdate(RequestId, Percent, Message, bStillWorking);
    }
}

void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(
    const FString& RequestId,
    const bool bSuccess,
    const FString& Message,
    const FString& ErrorCode)
{
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->RecordAutomationTelemetry(
            RequestId,
            bSuccess,
            Message,
            ErrorCode);
    }
}
