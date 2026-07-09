#include "McpAutomationBridgeSubsystem.h"

#include "MCP/Transport/McpNativeTransport.h"
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

    ERequestOrigin EffectiveOrigin =
        Origin == ERequestOrigin::WebSocket ? CurrentRequestOrigin : Origin;
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
