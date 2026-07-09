#include "McpAutomationBridgeSubsystem.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"

bool UMcpAutomationBridgeSubsystem::CancelAutomationRequest(
    const FString& RequestId)
{
    TArray<FString> RequestIds;
    RequestIds.Add(RequestId);
    return CancelAutomationRequests(RequestIds);
}

bool UMcpAutomationBridgeSubsystem::CancelAutomationRequests(
    const TArray<FString>& RequestIds)
{
    if (RequestIds.IsEmpty())
    {
        return false;
    }

    TSet<FString> UniqueRequestIds;
    UniqueRequestIds.Reserve(RequestIds.Num());
    for (const FString& RequestId : RequestIds)
    {
        UniqueRequestIds.Add(RequestId);
    }
    int32 RemovedCount = 0;
    std::atomic<bool> bNeedsExecutionBarrier{false};
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        RemovedCount = PendingAutomationRequests.RemoveAll(
            [&UniqueRequestIds](const FPendingAutomationRequest& Request)
            {
                return UniqueRequestIds.Contains(Request.RequestId);
            });
        for (const FString& RequestId : UniqueRequestIds)
        {
            const bool bWasInFlight =
                InFlightAutomationRequestIds.Contains(RequestId);
            const bool bWasActive =
                ActiveAutomationRequestIds.Contains(RequestId);
            if (bWasInFlight || bWasActive)
            {
                bNeedsExecutionBarrier.store(true, std::memory_order_release);
            }
            if (bWasInFlight)
            {
                CanceledAutomationRequestIds.Add(RequestId);
            }
        }
    }

    // Acquire the execution mutex as a barrier to ensure ProcessPendingAutomationRequests
    // observes the cancellation flags above before it begins processing the next request.
    if (bNeedsExecutionBarrier.load(std::memory_order_acquire))
    {
        FScopeLock ExecutionLock(&AutomationRequestExecutionMutex);
    }

    TArray<TFunction<void()>> CancellationCallbacks;
    {
        // Second critical section: extracts cancellation callbacks and cleans
        // up the CanceledAutomationRequestIds set once the request is no
        // longer in-flight or active. This block is intentionally separate
        // from the first one because:
        //   1. The execution-barrier mutex (AutomationRequestExecutionMutex)
        //      must be acquired BETWEEN the two blocks — holding both the
        //      request mutex and the execution mutex at once would defeat
        //      the barrier's purpose.
        //   2. The callbacks returned from RemoveAndCopyValue are invoked
        //      OUTSIDE this block (see the for-loop below). Holding the
        //      request mutex while invoking a callback that re-enters the
        //      queue (e.g. to clear cancellation state) would deadlock.
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        for (const FString& RequestId : UniqueRequestIds)
        {
            TFunction<void()> CancellationCallback;
            if (AutomationRequestCancellationCallbacks.RemoveAndCopyValue(
                    RequestId, CancellationCallback))
            {
                CancellationCallbacks.Add(MoveTemp(CancellationCallback));
            }
            if (!InFlightAutomationRequestIds.Contains(RequestId) &&
                !ActiveAutomationRequestIds.Contains(RequestId))
            {
                CanceledAutomationRequestIds.Remove(RequestId);
            }
        }
    }

    for (TFunction<void()>& CancellationCallback : CancellationCallbacks)
    {
        // Run the callback on the game thread to avoid races with
        // game-thread-only state. The off-thread path uses a sync
        // FEvent::Wait(), which is a known deadlock hazard if the
        // callback itself dispatches work back to this same queue
        // (the game thread would never free up to run it). Callbacks
        // registered via RegisterAutomationRequestCancellation MUST
        // therefore be lightweight, non-blocking, and must not re-enter
        // the request queue. If a callback needs significant work, it
        // should schedule it via AsyncTask and return immediately.
        if (IsInGameThread())
        {
            CancellationCallback();
        }
        else
        {
            FEvent* CompletionEvent =
                FPlatformProcess::GetSynchEventFromPool(true);
            AsyncTask(
                ENamedThreads::GameThread,
                [Callback = MoveTemp(CancellationCallback),
                 CompletionEvent]() mutable
                {
                    Callback();
                    CompletionEvent->Trigger();
                });
            CompletionEvent->Wait();
            FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
        }
    }
    return RemovedCount > 0 || bNeedsExecutionBarrier ||
        !CancellationCallbacks.IsEmpty();
}

bool UMcpAutomationBridgeSubsystem::CancelAllAutomationRequests()
{
    TArray<FString> RequestIds;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        RequestIds.Reserve(
            PendingAutomationRequests.Num() +
            InFlightAutomationRequestIds.Num() +
            ActiveAutomationRequestIds.Num() +
            AutomationRequestCancellationCallbacks.Num());
        for (const FPendingAutomationRequest& Request :
             PendingAutomationRequests)
        {
            RequestIds.Add(Request.RequestId);
        }
        RequestIds.Append(InFlightAutomationRequestIds.Array());
        RequestIds.Append(ActiveAutomationRequestIds.Array());
        TArray<FString> CallbackRequestIds;
        AutomationRequestCancellationCallbacks.GenerateKeyArray(
            CallbackRequestIds);
        RequestIds.Append(CallbackRequestIds);
    }
    return CancelAutomationRequests(RequestIds);
}

bool UMcpAutomationBridgeSubsystem::RegisterAutomationRequestCancellation(
    const FString& RequestId,
    TFunction<void()> Callback)
{
    if (RequestId.IsEmpty() || !Callback)
    {
        return false;
    }
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    if (CanceledAutomationRequestIds.Contains(RequestId))
    {
        return false;
    }
    AutomationRequestCancellationCallbacks.Add(
        RequestId, MoveTemp(Callback));
    return true;
}

void UMcpAutomationBridgeSubsystem::ClearAutomationRequestCancellation(
    const FString& RequestId)
{
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    AutomationRequestCancellationCallbacks.Remove(RequestId);
    CanceledAutomationRequestIds.Remove(RequestId);
}

void UMcpAutomationBridgeSubsystem::DiscardCanceledAutomationRequest(
    const FString& RequestId)
{
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    CanceledAutomationRequestIds.Remove(RequestId);
}
