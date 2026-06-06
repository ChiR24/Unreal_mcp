#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_ProcessRequestDispatch.h"
#include "McpConnectionManager.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    ERequestOrigin Origin) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> ProcessAutomationRequest ENTRY: RequestId=%s action='%s' "
              "(thread=%s)"),
         *RequestId, *Action,
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s "
              "action=%s activeSockets=%d"),
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"),
         *RequestId, *Action,
         ConnectionManager.IsValid() ? ConnectionManager->GetActiveSocketCount()
                                     : 0);
  if (!IsInGameThread()) {
    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
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

    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s "
              "action=%s bProcessingAutomationRequest=%s"),
         *RequestId, *Action,
         bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

  const FString LowerAction = Action.ToLower();

  if (ConnectionManager.IsValid()) {
    ConnectionManager->StartRequestTelemetry(RequestId, Action);
  }

  // Reentrancy guard / enqueue
  if (bProcessingAutomationRequest) {
    QueueAutomationRequest(RequestId, Action, Payload, RequestingSocket, Origin);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Enqueued automation request %s for action %s (processing in "
                "progress)."),
           *RequestId, *Action);
    return;
  }

  bProcessingAutomationRequest = true;
  CurrentRequestOrigin = Origin;
  bool bDispatchHandled = false;
  bool bErrorCaptureStarted = false;
  FString ConsumedHandlerLabel = TEXT("unknown-handler");
  const double DispatchStartSeconds = FPlatformTime::Seconds();

  auto HandleAndLog = [&](const TCHAR *HandlerLabel, auto &&Callable) -> bool {
    const bool bResult = Callable();
    if (bResult) {
      bDispatchHandled = true;
      ConsumedHandlerLabel = HandlerLabel;
    }
    return bResult;
  };

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
      CurrentRequestOrigin = ERequestOrigin::WebSocket;
      const double DispatchEndSeconds = FPlatformTime::Seconds();
      const double DurationMs =
          (DispatchEndSeconds - DispatchStartSeconds) * 1000.0;
      if (bDispatchHandled) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("ProcessAutomationRequest: Completed handler='%s' "
                    "RequestId=%s action='%s' (%.3f ms) engineErrors=%s"),
               *ConsumedHandlerLabel, *RequestId, *Action, DurationMs,
               bHadEngineErrors ? TEXT("true") : TEXT("false"));
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: No handler consumed "
                    "RequestId=%s action='%s' (%.3f ms)"),
               *RequestId, *Action, DurationMs);
      }
    };

    try {
      if (LowerAction == TEXT("manage_logs")) {
        if (HandleAndLog(TEXT("HandleLogAction (direct)"), [&]() {
              return HandleLogAction(RequestId, Action, Payload,
                                     RequestingSocket);
            }))
          return;
      }

      // =========================================================================
      // Begin Error Capture for this request (inside try block)
      // =========================================================================
      // This captures engine-level errors (like ensure failures) that occur
      // during handler execution. SendAutomationResponse checks the capture and
      // turns otherwise successful responses into ENGINE_ERROR failures so tool
      // responses stay aligned with the Unreal log.
      // Note: BeginErrorCapture is placed inside the try block to avoid
      // capturing our own catch-block error logging.
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
        if (HandleAndLog(*Action, [&]() {
              return (*Handler)(RequestId, Action, Payload, RequestingSocket);
            })) {
          return;
        }
      }

      if (McpProcessRequestDispatch::DispatchFallbackAutomationRequest(
              this, RequestId, Action, LowerAction, Payload, RequestingSocket,
              ConsumedHandlerLabel)) {
        bDispatchHandled = true;
        return;
      }

      // Unhandled action
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("SendAutomationError (unknown action)");
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown automation action: %s"), *Action),
          TEXT("UNKNOWN_ACTION"));
    } catch (const std::exception &E) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Unhandled exception processing automation request %s: %s"),
             *RequestId, ANSI_TO_TCHAR(E.what()));
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler");
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())),
          TEXT("INTERNAL_ERROR"));
    } catch (...) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Error,
          TEXT("Unhandled unknown exception processing automation request %s"),
          *RequestId);
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler (unknown)");
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Internal error (unknown)."),
                          TEXT("INTERNAL_ERROR"));
    }
  }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
