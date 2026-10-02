#include "McpAutomationBridgeSubsystem.h"

#include "Editor.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Core/Security/McpPrequeueGate.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseEnrichment.h"
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
// An error CODE is a SCREAMING_SNAKE token. Anything else in that slot is a
// human sentence a dispatch wrapper forwarded out of the handler's `error`
// field, which is not the same thing.
bool LooksLikeErrorCode(const FString& Candidate)
{
    for (const TCHAR Ch : Candidate)
    {
        if ((Ch < TEXT('A') || Ch > TEXT('Z')) && (Ch < TEXT('0') || Ch > TEXT('9')) && Ch != TEXT('_')) { return false; }
    }
    return !Candidate.IsEmpty();
}

}

void UMcpAutomationBridgeSubsystem::BroadcastAutomationEvent(const TSharedPtr<FJsonObject>& Event)
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

    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendRawMessageToLogSubscribers(SerializedEvent);
    }

    if (NativeTransport)
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
    // A batch running this handler in-process wants the reply as data, not on a wire.
    if (FMcpResponseCaptureRegistry::Get().TryCapture(RequestId, bSuccess, Message, Result, ErrorCode)) { return; }

    ClearAutomationRequestCancellation(RequestId);

    // The gate burns the caller's single-use consent grant BEFORE the handler
    // runs, so a handler that refuses -- a misspelled component name, a path
    // that resolves to nothing -- used to cost the grant for a call that
    // changed nothing, and the retry needed a fresh describe. Hand it back on
    // any failure; a call that succeeded keeps the burn, so replay protection
    // is unchanged.
    if (bSuccess) { McpPrequeueGate::ForgetConsentForRequest(RequestId); }
    else { McpPrequeueGate::RefundConsentForRequest(RequestId); }

    FString EffectiveMessage = Message;
    FString EffectiveErrorCode = ErrorCode;
    TSharedPtr<FJsonObject> EffectiveResult = Result;

    if (bSuccess && bProcessingAutomationRequest)
    {
        // Warnings were captured alongside errors and then never read, so every
        // "it worked, but..." the engine logged died at this line. Both are
        // copied unconditionally now; an empty capture copies empty arrays.
        FRequestErrorCapture Captured;
        {
            FScopeLock Lock(&ErrorCaptureMutex);
            Captured = CurrentErrorCapture;
        }

        // Name the world this request ran against. An actor mutation reports success for
        // whichever world was current at that instant; if a level load then replaces it, the actor is
        // unreachable and the receipt gives no hint. Reporting the world (and flagging a transient
        // /Temp one) makes that detectable. PIE context wins when present, since that is the world an
        // actor-facing request actually touched.
        FString WorldName;
        bool bTransientWorld = false;
        UWorld* ContextWorld = nullptr;
        if (GEditor)
        {
            if (const FWorldContext* PieContext = GEditor->GetPIEWorldContext())
            {
                ContextWorld = PieContext->World();
            }
            if (!ContextWorld)
            {
                ContextWorld = GEditor->GetEditorWorldContext().World();
            }
        }
        if (ContextWorld)
        {
            const UPackage* WorldPackage = ContextWorld->GetOutermost();
            WorldName = WorldPackage ? WorldPackage->GetName() : ContextWorld->GetName();
            bTransientWorld = WorldName.StartsWith(TEXT("/Temp/"));
        }

        EffectiveResult = McpBuildEnrichedResponseResult(Result, WorldName, bTransientWorld, Captured);
    }

    // Dozens of dispatch wrappers hand the handler's `error` sentence straight
    // to the ErrorCode parameter, so a failure rendered as
    // `Error [Parent component not found: X]: execute failed` - the message in
    // the code slot and a placeholder in the message slot. Normalizing at the
    // one funnel every response passes through fixes all of them at once.
    if (!bSuccess && !LooksLikeErrorCode(EffectiveErrorCode))
    {
        if (EffectiveMessage.IsEmpty()) { EffectiveMessage = EffectiveErrorCode; }
        EffectiveErrorCode.Reset();
    }
    if (!bSuccess)
    {
        EffectiveMessage = SanitizeEngineErrorForResponse(EffectiveMessage);
        FScopeLock Lock(&ErrorCaptureMutex);
        McpAppendPieRefusalHint(EffectiveMessage, bProcessingAutomationRequest && GEditor && GEditor->PlayWorld, CurrentErrorCapture.ErrorMessages);
    }

    // The registry wins over CurrentRequestOrigin because it is the only source
    // still true for a DEFERRED reply: ProcessAutomationRequest's ON_SCOPE_EXIT
    // resets the global to WebSocket, so a handler answering from an
    // AsyncTask/timer/delegate reached this line with it already cleared, and
    // every native /mcp response from such a handler went down the WebSocket
    // path, was dropped, and hung the caller until the 300s SSE sweeper.
    ERequestOrigin RecordedOrigin = ERequestOrigin::WebSocket;
    const ERequestOrigin EffectiveOrigin = FMcpRequestOriginRegistry::Get().Resolve(RequestId, RecordedOrigin)
        ? RecordedOrigin
        : (Origin == ERequestOrigin::WebSocket ? CurrentRequestOrigin : Origin);
    // Released here: the single funnel every delivered response passes through.
    FMcpRequestOriginRegistry::Get().Forget(RequestId);
    // Bounded terminal in the single response funnel, before the
    // transport branch. Persist inline ONLY on the game thread (deferred
    // replies coalesce to the next game-thread persist); the native branch
    // below RETURNS, so this must precede it.
    FMcpDiagnosticsSnapshot::Get().RecordTerminal(RequestId, bSuccess ? TEXT("success") : EffectiveErrorCode.IsEmpty() ? TEXT("failure") : EffectiveErrorCode);
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
        // asset paths and object names. Through the connection manager, which also drops
        // the action it recorded at dispatch: calling the registry directly left one
        // ActiveRequestActions entry behind per native request, for the editor's lifetime.
        if (ConnectionManager.IsValid())
        {
            ConnectionManager->RecordAutomationTelemetry(RequestId, bSuccess, FString(), EffectiveErrorCode);
        }
        else
        {
            FMcpTelemetryRegistry::Get().EndRequest(
                RequestId,
                bSuccess ? TEXT("success") : TEXT("failure"),
                EffectiveErrorCode);
        }
        if (!NativeTransport->CompletePendingRequest(
                RequestId,
                bSuccess,
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
            bSuccess,
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
    const FString ResolvedError = ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    // A batch step's failure is data in the batch's own reply; logged, the outer request's warning
    // capture repeated it under warnings beside the batch's own list of misses.
    if (!FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request failed (%s): %s"),
               *ResolvedError, *SanitizeForLog(Message));
    }
    SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::SendAutomationRejection(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
    const FString& RequestId,
    EAutomationQueueRejection Reason)
{
    FString Code;
    FString Message;
    DescribeQueueRejection(Reason, Code, Message);
    SendAutomationError(TargetSocket, RequestId, Message, Code);
}

void UMcpAutomationBridgeSubsystem::DescribeQueueRejection(
    EAutomationQueueRejection Reason, FString& Code, FString& Message)
{
    Code = TEXT("AUTOMATION_REQUEST_REJECTED");
    Message = TEXT("Automation request rejected");
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
        case EAutomationQueueRejection::GameThreadStalled:
        {
            // Say what holds the editor and how far it has got (read off the game thread), not only that it is held.
            Code = TEXT("EDITOR_BLOCKED");
            const FString Work = McpAutomationBridge::DescribeEditorWork();
            Message = Work.IsEmpty()
                ? FString(TEXT("Automation request rejected: the editor game thread has not ticked for over 15 s (a modal dialog or a blocking operation is holding it); dismiss it and retry"))
                : FString::Printf(TEXT("Not run: Unreal is %s. Send this call again once that is done; until then every call answers with its progress."), *Work);
            break;
        }
        case EAutomationQueueRejection::SessionQueueFull:
            Code = TEXT("AUTOMATION_SESSION_QUEUE_FULL");
            Message = TEXT("Automation request rejected: this session already has the maximum number of queued requests; retry after your queued work drains");
            break;
        default:
            break;
    }
}

void UMcpAutomationBridgeSubsystem::SendProgressUpdate(
    const FString& RequestId,
    float Percent,
    const FString& Message,
    bool bStillWorking,
    ERequestOrigin /*Origin*/)
{
    McpAutomationBridge::ReportInFlightProgress(RequestId, Percent, Message);
    // Each transport only writes to a request it holds, so both are told: routing by Origin starved a native
    // client whenever a handler left Origin at its WebSocket default (the thumbnail and LOD handlers did).
    if (NativeTransport)
    {
        NativeTransport->SendSSEProgressUpdate(RequestId, Percent, Message);
    }
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->SendProgressUpdate(RequestId, Percent, Message, bStillWorking);
    }
}

