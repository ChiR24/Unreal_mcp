#include "McpAutomationBridgeSubsystem.h"

#include "Async/Async.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Foundation/McpLiveStateRevisions.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "Misc/ScopeExit.h"

EAutomationQueueRejection UMcpAutomationBridgeSubsystem::QueueAutomationRequest(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    ERequestOrigin Origin,
    const TMap<EMcpStateKind, int64>& ExpectedRevisions,
    const FString& SessionKey)
{
    FPendingAutomationRequest Pending;
    Pending.RequestId = RequestId;
    Pending.Action = Action;
    Pending.Payload = Payload;
    Pending.RequestingSocket = RequestingSocket;
    Pending.Origin = Origin;
    Pending.ExpectedRevisions = ExpectedRevisions;
    Pending.SessionKey = McpQueueFairness::ResolveSessionKey(
        SessionKey, RequestingSocket.Get());

    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (!bAcceptingAutomationRequests)
        {
            return EAutomationQueueRejection::NotAccepting;
        }
        if (CanceledAutomationRequestIds.Remove(RequestId) > 0)
        {
            return EAutomationQueueRejection::AlreadyCanceled;
        }
        // Per-session admission cap, checked BEFORE the global cap so a
        // flooding session gets the precise refusal instead of a generic
        // QueueFull, and so it can never consume every global slot and starve
        // sessions that have nothing queued at all. The anonymous lane is
        // exempt: no remote client can reach it (a WebSocket request always
        // carries its socket, a native MCP request always carries its session
        // id), it only receives in-process re-queues from the save/GC/async-load
        // deferral path, and refusing those would turn a deferral into a lost
        // request. It stays bounded by the global cap below.
        if (Pending.SessionKey != McpQueueFairness::AnonymousSessionKey())
        {
            int32 SessionPendingNum = 0;
            for (const FPendingAutomationRequest& Queued : PendingAutomationRequests)
            {
                if (Queued.SessionKey == Pending.SessionKey)
                {
                    ++SessionPendingNum;
                }
            }
            if (SessionPendingNum >=
                FMcpQueueFairnessState::MaxPendingRequestsPerSession)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("Session automation queue is full (%d pending); rejecting action=%s"),
                    SessionPendingNum,
                    *Action);
                return EAutomationQueueRejection::SessionQueueFull;
            }
        }
        if (PendingAutomationRequests.Num() >= MaxPendingAutomationRequests)
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Automation request queue is full; rejecting action=%s"),
                *Action);
            return EAutomationQueueRejection::QueueFull;
        }
        PendingAutomationRequests.Add(MoveTemp(Pending));
    }

    // The origin has to outlive the synchronous dispatch that consumes this
    // request: a handler that defers its reply answers after
    // ProcessAutomationRequest has already reset CurrentRequestOrigin, and
    // without this record its response is routed to the wrong transport and
    // dropped. Recorded by the admitting caller, so it states the truth of THIS
    // request rather than inferring an owner from a pending-id lookup.
    FMcpRequestOriginRegistry::Get().Record(RequestId, Origin);

    // Opens the queue-wait interval at ADMISSION. The matching MarkDispatched
    // below closes it at dispatch, so queue wait is a measured delta between two
    // reads of one injectable clock rather than an estimate.
    FMcpTelemetryRegistry::Get().BeginRequest(RequestId, FString());

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Verbose,
        TEXT("Queued automation request for core ticker: RequestId=%s action=%s"),
        *RequestId,
        *Action);
    return EAutomationQueueRejection::None;
}

void UMcpAutomationBridgeSubsystem::StartAcceptingAutomationRequests()
{
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    bAcceptingAutomationRequests = true;
}

void UMcpAutomationBridgeSubsystem::StopAcceptingAutomationRequests()
{
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    bAcceptingAutomationRequests = false;
}

void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests()
{
    if (!IsInGameThread())
    {
        AsyncTask(
            ENamedThreads::GameThread,
            [this]() { this->ProcessPendingAutomationRequests(); });
        return;
    }

    // Single mutation lane, guard 1 of 2: a nested drain is refused outright.
    // AutomationRequestExecutionMutex cannot carry this — FCriticalSection is
    // RECURSIVE, so the same game thread re-entering it (a handler that pumps
    // the ticker while it runs) would sail straight through and dequeue a
    // second request INSIDE the first one's dispatch. That is the only way this
    // scheduler could ever open a second lane, so it is closed here.
    if (QueueFairness.bDraining)
    {
        return;
    }
    TGuardValue<bool> DrainGuard(QueueFairness.bDraining, true);

    TArray<FPendingAutomationRequest> LocalQueue;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (PendingAutomationRequests.Num() == 0)
        {
            return;
        }
        TArray<FString> SessionKeys;
        SessionKeys.Reserve(PendingAutomationRequests.Num());
        for (const FPendingAutomationRequest& Queued : PendingAutomationRequests)
        {
            SessionKeys.Add(Queued.SessionKey);
        }
        const int32 BatchSize =
            FMath::Min(MaxAutomationRequestsPerTick,
                       PendingAutomationRequests.Num());
        TArray<int32> SelectedIndices;
        McpQueueFairness::SelectFairBatch(
            SessionKeys, BatchSize, QueueFairness.LastServedSessionKey,
            SelectedIndices);
        LocalQueue.Reserve(SelectedIndices.Num());
        for (const int32 Index : SelectedIndices)
        {
            LocalQueue.Add(PendingAutomationRequests[Index]);
            InFlightAutomationRequestIds.Add(
                PendingAutomationRequests[Index].RequestId);
        }
        if (LocalQueue.Num() > 0)
        {
            QueueFairness.LastServedSessionKey = LocalQueue.Last().SessionKey;
        }
        // Descending so each removal cannot shift an index still to be removed.
        SelectedIndices.Sort([](const int32 A, const int32 B) { return A > B; });
        for (const int32 Index : SelectedIndices)
        {
            PendingAutomationRequests.RemoveAt(
                Index, 1, MCP_DISALLOW_SHRINKING);
        }
    }

    for (const FPendingAutomationRequest& Req : LocalQueue)
    {
        // Lock-ordering invariant: AutomationRequestExecutionMutex and
        // PendingAutomationRequestsMutex MUST remain strictly sequential here.
        // They are never held simultaneously. McpAutomationBridgeSubsystemRequestQueueCancellation.cpp
        // and the lock-order block in McpNativeTransport.h rely on this — any
        // future change that nests them risks deadlock.
        //
        // The block below follows the same three-step pattern as
        // CancelAutomationRequests (Pending → Execution → Pending):
        //   1. Acquire Pending, decide whether to skip (canceled) or mark
        //      active, then RELEASE Pending.
        //   2. Acquire Execution as the cancel barrier, run the request,
        //      RELEASE Execution.
        //   3. Acquire Pending, remove from active set, RELEASE Pending.
        // This keeps the two critical sections strictly sequential.
        bool bSkip = false;
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            if (CanceledAutomationRequestIds.Remove(Req.RequestId) > 0)
            {
                InFlightAutomationRequestIds.Remove(Req.RequestId);
                bSkip = true;
            }
            else
            {
                ActiveAutomationRequestIds.Add(Req.RequestId);
            }
        }
        if (bSkip)
        {
            continue;
        }
        // Task 42 authoritative precondition gate. It runs HERE — on the game
        // thread, after the queue wait, immediately before dispatch — so a state
        // change that lands between enqueue and dispatch still refuses. Checking
        // at parse or transport time would admit a request the editor has already
        // invalidated. Origin is passed explicitly because CurrentRequestOrigin
        // is only set inside the ProcessAutomationRequest we are skipping.
        EMcpStateKind StaleKind = EMcpStateKind::Selection;
        int64 StaleExpected = 0;
        int64 StaleCurrent = 0;
        if (Req.ExpectedRevisions.Num() > 0 &&
            !FMcpLiveStateRevisions::Get().CheckPreconditions(
                Req.ExpectedRevisions, StaleKind, StaleExpected, StaleCurrent))
        {
            SendAutomationResponse(
                Req.RequestingSocket, Req.RequestId, false,
                FString::Printf(
                    TEXT("Editor '%s' state changed since it was read (expected %lld, current %lld). Re-read the state and retry."),
                    FMcpLiveStateRevisions::KeyFor(StaleKind), StaleExpected, StaleCurrent),
                nullptr, FMcpLiveStateRevisions::StaleStateErrorCode(), Req.Origin);
            {
                FScopeLock Lock(&PendingAutomationRequestsMutex);
                ActiveAutomationRequestIds.Remove(Req.RequestId);
                InFlightAutomationRequestIds.Remove(Req.RequestId);
            }
            continue;
        }
        {
            FScopeLock ExecutionLock(&AutomationRequestExecutionMutex);
            // Single mutation lane, guard 2 of 2: the lane is asserted, not
            // assumed. Depth is raised only here, around the one call that
            // mutates the editor, so a depth above 1 means two mutations are
            // in flight at once. MaxObservedDispatchDepth keeps the high-water
            // mark after the depth unwinds so a test can prove the lane held
            // for a whole run without relying on the assert firing.
            ++QueueFairness.DispatchDepth;
            QueueFairness.MaxObservedDispatchDepth = FMath::Max(
                QueueFairness.MaxObservedDispatchDepth,
                QueueFairness.DispatchDepth);
            ON_SCOPE_EXIT { --QueueFairness.DispatchDepth; };
            FMcpTelemetryRegistry::Get().MarkDispatched(Req.RequestId);
            checkf(
                QueueFairness.DispatchDepth == 1 && IsInGameThread(),
                TEXT("MCP editor mutation lane violated: depth=%d gameThread=%d"),
                QueueFairness.DispatchDepth,
                IsInGameThread() ? 1 : 0);
            // The pins and the session lane travel WITH the request. If the
            // dispatch below defers again (GC / async load) or hits the
            // reentrancy guard, ProcessAutomationRequest re-queues, and passing
            // these through is what stops that second hop from silently
            // dropping the live-state precondition and the session identity.
            ProcessAutomationRequest(
                Req.RequestId,
                Req.Action,
                Req.Payload,
                Req.RequestingSocket,
                Req.Origin,
                Req.ExpectedRevisions,
                Req.SessionKey);
        }
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            ActiveAutomationRequestIds.Remove(Req.RequestId);
            InFlightAutomationRequestIds.Remove(Req.RequestId);
            CanceledAutomationRequestIds.Remove(Req.RequestId);
        }
    }
}
