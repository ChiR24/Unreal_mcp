#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Core/Security/McpPrequeueGate.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpConnectionManager.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Framework/Application/SlateApplication.h" // active-modal pre-flight (Slate is an editor dep)
#include "Foundation/McpScreenshotResample.h" // McpShaderJobsRemaining

// Publish the currently-executing automation action, when it started and its last progress, so a reader off the
// game thread (the busy refusal, the per-request keep-alive) can say what Unreal is working on while a handler
// blocks the game thread. The lock is held ONLY for the brief set/get — never during the handler. Declared in the
// public McpAutomationBridgeSubsystem.h so other modules can call it.
namespace McpAutomationBridge
{
    static FCriticalSection GInFlightActionCS;
    static FString GInFlightAction;
    static FString GInFlightRequestId;
    static double GInFlightStart = 0.0;
    static double GInFlightProgressTime = 0.0;
    static float GInFlightPercent = -1.0f;
    static FString GInFlightProgress;
    static void SetInFlightAction(const FString& InRequestId, const FString& InAction)
    {
        FScopeLock Lock(&GInFlightActionCS);
        GInFlightAction = InAction;
        GInFlightRequestId = InRequestId;
        GInFlightStart = InAction.IsEmpty() ? 0.0 : FPlatformTime::Seconds();
        GInFlightProgressTime = 0.0;
        GInFlightPercent = -1.0f;
        GInFlightProgress.Reset();
    }
    MCPAUTOMATIONBRIDGE_API FString GetInFlightAction()
    {
        FScopeLock Lock(&GInFlightActionCS);
        return GInFlightAction;
    }
    MCPAUTOMATIONBRIDGE_API void ReportInFlightProgress(const FString& RequestId, float Percent, const FString& Message)
    {
        FScopeLock Lock(&GInFlightActionCS);
        if (GInFlightAction.IsEmpty() || RequestId != GInFlightRequestId) return;
        GInFlightProgressTime = FPlatformTime::Seconds();
        GInFlightPercent = Percent;
        if (!Message.IsEmpty()) GInFlightProgress = Message;
    }
    MCPAUTOMATIONBRIDGE_API FString DescribeEditorWork(const FString& ForRequestId)
    {
        const double Now = FPlatformTime::Seconds();
        FString Work;
        {
            FScopeLock Lock(&GInFlightActionCS);
            if (!GInFlightAction.IsEmpty())
            {
                const bool bOwn = !ForRequestId.IsEmpty() && ForRequestId == GInFlightRequestId;
                Work = FString::Printf(TEXT("%s %s for %.0f s"), bOwn ? TEXT("working on") : TEXT("busy with"),
                    *GInFlightAction, Now - GInFlightStart);
                if (!GInFlightProgress.IsEmpty())
                {
                    Work += GInFlightPercent >= 0.0f
                        ? FString::Printf(TEXT(", %.0f%% done: %s"), GInFlightPercent, *GInFlightProgress)
                        : FString::Printf(TEXT(": %s"), *GInFlightProgress);
                    Work += FString::Printf(TEXT(" (reported %.0f s ago)"), Now - GInFlightProgressTime);
                }
            }
        }
        // The engine's own work, which blocks the game thread too (a delete or a material change recompiles shaders).
        const int32 Shaders = McpShaderJobsRemaining();
        if (Shaders > 0)
        {
            Work += FString::Printf(TEXT("%scompiling %d shaders"), Work.IsEmpty() ? TEXT("") : TEXT("; "), Shaders);
        }
        return Work;
    }
}

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    ERequestOrigin Origin,
    const TMap<EMcpStateKind, int64> &ExpectedRevisions,
    const FString &SessionKey) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s "
              "action=%s activeSockets=%d"),
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"),
         *RequestId, *Action,
         ConnectionManager.IsValid() ? ConnectionManager->GetActiveSocketCount()
                                     : 0);
  // Off the game thread, mid save/GC/load, or re-entrant: queue it (or refuse).
  const auto QueueOrRefuse = [&]() {
    const EAutomationQueueRejection Reason = QueueAutomationRequest(
        RequestId, Action, Payload, RequestingSocket, Origin,
        ExpectedRevisions, SessionKey);
    if (Reason != EAutomationQueueRejection::None) {
      SendAutomationRejection(RequestingSocket, RequestId, Reason);
    }
  };
  if (!IsInGameThread()) {
    QueueOrRefuse();
    return;
  }

  // Guard against unsafe engine states (Saving, GC, Async Loading)
  // Calling StaticFindObject (via ResolveClassByName) during these states can
  // cause crashes.
  if (GIsSavingPackage || IsGarbageCollecting() || IsAsyncLoading()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Deferring ProcessAutomationRequest due to active "
                "Serialization/GC/Loading: RequestId=%s Action=%s"),
           *RequestId, *Action);

    QueueOrRefuse();
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s "
              "action=%s bProcessingAutomationRequest=%s"),
         *RequestId, *Action,
         bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

  if (ConnectionManager.IsValid()) {
    ConnectionManager->StartRequestTelemetry(RequestId, Action);
  }

  // Reentrancy guard / enqueue
  if (bProcessingAutomationRequest) {
    QueueOrRefuse();
    return;
  }

  // if the editor is ALREADY blocked on a modal window, do NOT stack another handler on top of it — reply
  // with a clear signal instead of piling onto the freeze. Defensive: the primary K1 fix is the unattended-guard
  // below, which stops handlers from opening a blocking modal in the first place.
  if (FSlateApplication::IsInitialized() &&
      FSlateApplication::Get().GetActiveModalWindow().IsValid()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("ProcessAutomationRequest: refusing dispatch while a modal window is active "
                "RequestId=%s action='%s'"),
           *RequestId, *Action);
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Editor is waiting on a modal dialog; automation is paused until it is dismissed."),
        TEXT("EDITOR_MODAL_ACTIVE"));
    return;
  }

  // Dispatch really starts HERE - after the reentrancy guard and the modal
  // refusal, both of which re-queue or refuse without running a handler. Closing
  // the queue interval any earlier would attribute a second queue wait to
  // handler time. BeginRequest is idempotent on the start instant, so a queued
  // request keeps the admission timestamp and only gains its bounded action
  // class; a request dispatched inline opens a zero-length queue interval here.
  FMcpTelemetryRegistry::Get().BeginRequest(
      RequestId, McpPrequeueGate::ResolveActionClass(Action, Payload));
  FMcpTelemetryRegistry::Get().MarkDispatched(RequestId);

  bProcessingAutomationRequest = true;
  CurrentRequestOrigin = Origin;
  // force UE unattended-script mode for the duration of handler dispatch so any FMessageDialog / editor
  // confirmation prompt AUTO-ANSWERS its default instead of opening a BLOCKING modal that freezes the game thread
  // (the witnessed force-kill / data-loss class). RAII-restored on every function-scope exit (incl. the early
  // returns and the exception paths below). Non-differential stability fix.
  TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);
  // B11/K2b: record which action is now executing (cleared in the ON_SCOPE_EXIT below) so the off-thread watchdog
  // can name it if this handler freezes the game thread.
  {
    const FString SubAction = Payload.IsValid() ? GetJsonStringField(Payload, TEXT("subAction")) : FString();
    McpAutomationBridge::SetInFlightAction(RequestId, SubAction.IsEmpty() ? Action : Action + TEXT(" ") + SubAction);
  }
  bool bDispatchHandled = false;
  bool bErrorCaptureStarted = false;
  const double DispatchStartSeconds = FPlatformTime::Seconds();

  {
    ON_SCOPE_EXIT {
      // =====================================================================
      // End Error Capture and check for captured errors
      // =====================================================================
      TArray<FString> CapturedErrors;
      bool bHadEngineErrors = false;
      if (bErrorCaptureStarted)
      {
        CapturedErrors = EndErrorCapture();
        bHadEngineErrors = HasCapturedErrors();
      }

      if (bHadEngineErrors && bDispatchHandled)
      {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: Handler reported success but "
                    "engine errors were detected for RequestId=%s action='%s'. "
                    "Errors: %s"),
               *RequestId, *Action,
               CapturedErrors.Num() > 0 ? *FString::Join(CapturedErrors, TEXT("; ")) : TEXT("unknown"));

        // The handler response path converts successful responses to
        // ENGINE_ERROR failures when captured errors exist. Keep this warning as
        // a secondary audit trail for handlers that returned after logging an
        // engine error.
      }

      bProcessingAutomationRequest = false;
      McpAutomationBridge::SetInFlightAction(FString(), FString()); // clear the in-flight action on every exit path
      CurrentRequestOrigin = ERequestOrigin::WebSocket;
      const double DispatchEndSeconds = FPlatformTime::Seconds();
      const double DurationMs =
          (DispatchEndSeconds - DispatchStartSeconds) * 1000.0;
      if (bDispatchHandled) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("ProcessAutomationRequest: Completed "
                    "RequestId=%s action='%s' (%.3f ms) engineErrors=%s"),
               *RequestId, *Action, DurationMs,
               bHadEngineErrors ? TEXT("true") : TEXT("false"));
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: No handler consumed "
                    "RequestId=%s action='%s' (%.3f ms)"),
               *RequestId, *Action, DurationMs);
        // Nothing consumed the request and no response was sent, so the
        // connection-manager terminal path will never fire for it. Closing it
        // here is what stops this outcome from staying log-only. EndRequest is a
        // no-op once a terminal has already been recorded, so the handled paths
        // above cannot be double counted.
        FMcpTelemetryRegistry::Get().EndRequest(
            RequestId, TEXT("failure"), TEXT("internal"));
      }
    };

    // =========================================================================
    // Begin Error Capture for this request
    // =========================================================================
    // This captures engine-level errors (like ensure failures) that occur
    // during handler execution. SendAutomationResponse checks the capture and
    // turns otherwise successful responses into ENGINE_ERROR failures so tool
    // responses stay aligned with the Unreal log.
    BeginErrorCapture();
    bErrorCaptureStarted = true;

    // Map this requestId to the requesting socket so responses can be
    // delivered reliably
    if (!RequestId.IsEmpty() && RequestingSocket.IsValid() &&
        ConnectionManager.IsValid()) {
      ConnectionManager->RegisterRequestSocket(RequestId, RequestingSocket);
    }

    // ---------------------------------------------------------
    // Check Handler Registry (O(1) dispatch)
    // ---------------------------------------------------------
    if (const FAutomationHandler *Handler = AutomationHandlers.Find(Action)) {
      if ((*Handler)(RequestId, Action, Payload, RequestingSocket)) {
        bDispatchHandled = true;
        return;
      }
    }

    // Unhandled action
    bDispatchHandled = true;
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Unknown automation action: %s"), *Action),
        TEXT("UNKNOWN_ACTION"));
  }
}
