#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // This large implementation was extracted from the original subsystem
    // translation unit to keep the core file smaller and focused. It
    // contains the main dispatcher that delegates to specialized handler
    // functions (python/property/blueprint/sequence handlers) and retains
    // the queuing/scope-exit safety logic expected by callers.

    // Ensure automation processing happens on the game thread
    // This trace is intentionally verbose — routine requests can be high
    // frequency and will otherwise flood the logs. Developers can enable
    // Verbose logging to see these messages when required.
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s action=%s activeSockets=%d pendingQueue=%d"),
        IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"), *RequestId, *Action, ActiveSockets.Num(), PendingAutomationRequests.Num());
    if (!IsInGameThread())
    {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Scheduling ProcessAutomationRequest on GameThread: RequestId=%s action=%s"), *RequestId, *Action);
        AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(this), RequestId, Action, Payload, RequestingSocket]()
        {
            if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
            {
                Pinned->ProcessAutomationRequest(RequestId, Action, Payload, RequestingSocket);
            }
        });
        return;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s action=%s bProcessingAutomationRequest=%s"), *RequestId, *Action, bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

    // Diagnostic convenience
    const FString LowerAction = Action.ToLower();

    // Reentrancy guard / enqueue
    if (bProcessingAutomationRequest)
    {
        FPendingAutomationRequest P;
        P.RequestId = RequestId;
        P.Action = Action;
        P.Payload = Payload;
        P.RequestingSocket = RequestingSocket;
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            PendingAutomationRequests.Add(MoveTemp(P));
            bPendingRequestsScheduled = true;
        }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Enqueued automation request %s for action %s (processing in progress)."), *RequestId, *Action);
        return;
    }

    bProcessingAutomationRequest = true;
    {
        ON_SCOPE_EXIT
        {
            bProcessingAutomationRequest = false;
            if (bPendingRequestsScheduled)
            {
                bPendingRequestsScheduled = false;
                ProcessPendingAutomationRequests();
            }
        };

        try
        {
            // Allow small handlers to short-circuit fast (python/property)
            if (HandleExecuteEditorPython(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleExecuteEditorFunction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSetObjectProperty(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleGetObjectProperty(RequestId, Action, Payload, RequestingSocket)) return;

            // Delegate asset/control/blueprint/sequence actions to their handlers
            if (HandleAssetAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleControlActorAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleControlEditorAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleBlueprintAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSequenceAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleEffectAction(RequestId, Action, Payload, RequestingSocket)) return;

            // Unhandled action
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled exception processing automation request %s: %s"), *RequestId, ANSI_TO_TCHAR(E.what()));
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())), TEXT("INTERNAL_ERROR"));
        }
        catch (...)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled unknown exception processing automation request %s"), *RequestId);
            SendAutomationError(RequestingSocket, RequestId, TEXT("Internal error (unknown)."), TEXT("INTERNAL_ERROR"));
        }

    }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
