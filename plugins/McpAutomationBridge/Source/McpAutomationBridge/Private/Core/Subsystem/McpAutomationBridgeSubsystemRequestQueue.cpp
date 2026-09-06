#include "McpAutomationBridgeSubsystem.h"

#include "Async/Async.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Requests/McpRequestOriginRegistry.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
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

    EAutomationQueueRejection Rejection = EAutomationQueueRejection::None;
    int32 AdmissionDepthCarrier = 0;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (!bAcceptingAutomationRequests)
        {
            // BB-005: refuse while the lock is held so the queue-depth read is
            // race-free (same NF-3 discipline applied to admissions).
            FMcpDiagnosticsSnapshot::Get().RecordRefusal(RequestId, TEXT("AUTOMATION_NOT_ACCEPTING"), PendingAutomationRequests.Num());
            Rejection = EAutomationQueueRejection::NotAccepting;
        }
        else if (CanceledAutomationRequestIds.Remove(RequestId) > 0)
        {
            FMcpDiagnosticsSnapshot::Get().RecordRefusal(RequestId, TEXT("AUTOMATION_ALREADY_CANCELED"), PendingAutomationRequests.Num());
            Rejection = EAutomationQueueRejection::AlreadyCanceled;
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
        else if (Pending.SessionKey != McpQueueFairness::AnonymousSessionKey())
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
                FMcpDiagnosticsSnapshot::Get().RecordRefusal(RequestId, TEXT("AUTOMATION_SESSION_QUEUE_FULL"), PendingAutomationRequests.Num());
                Rejection = EAutomationQueueRejection::SessionQueueFull;
            }
        }
        if (Rejection == EAutomationQueueRejection::None &&
            PendingAutomationRequests.Num() >= MaxPendingAutomationRequests)
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Automation request queue is full; rejecting action=%s"),
                *Action);
            FMcpDiagnosticsSnapshot::Get().RecordRefusal(RequestId, TEXT("AUTOMATION_QUEUE_FULL"), PendingAutomationRequests.Num());
            Rejection = EAutomationQueueRejection::QueueFull;
        }
        if (Rejection == EAutomationQueueRejection::None)
        {
            // NF-2: the depth is captured IN the lock, as the last statement before
            // the Add, so the queue-depth read is race-free against socket threads.
            const int32 AdmissionDepth = PendingAutomationRequests.Num();
            AdmissionDepthCarrier = AdmissionDepth;
            PendingAutomationRequests.Add(MoveTemp(Pending));
        }
    }

    // BB-005 queue refusal: the refusal record is memory-only in-lock; its disk
    // write is deferred OFF the lock to the game thread, so a hard crash after
    // StopAcceptingAutomationRequests() (or on an empty queue) still leaves the
    // last refusal persisted (plan line 197). The admission path coalesces its
    // records into the next tick's pre-dispatch persist instead.
    if (Rejection != EAutomationQueueRejection::None)
    {
        FMcpDiagnosticsSnapshot::PersistCurrentAsync();
        return Rejection;
    }

    // BB-005: admission records memory-only on the socket/HTTP thread. The
    // depth was read in-lock above; these store calls take the store's own
    // mutex. NF-1: NO latch - every WebSocket admission records handshake
    // success (last-writer-wins under the store mutex, always fresh). An
    // admitted WS automation_request PROVES the bridge_hello gate passed for
    // THIS connection; a later 4004/4005 close (H6) overwrites ok=false.
    const int32 AdmissionDepth = AdmissionDepthCarrier;
    FMcpDiagnosticsSnapshot::Get().RecordAdmission(RequestId, FString(), Action,
        Origin == ERequestOrigin::NativeHTTP ? TEXT("NativeHTTP") : TEXT("WebSocket"), AdmissionDepth);
    if (Origin == ERequestOrigin::WebSocket) { FMcpDiagnosticsSnapshot::Get().RecordHandshake(true); }

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
    int32 PendingCountAfterBatchCarrier = 0;
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
        // NF-3: the post-batch count is captured IN the lock, after the
        // RemoveAt loop, so the depth a pre-dispatch record reports is the
        // race-free remainder seen by the mutation lane. Declared as a const
        // inside the lock; the carrier below carries it past the lock scope
        // because the pre-dispatch record runs after release.
        const int32 PendingCountAfterBatch = PendingAutomationRequests.Num();
        PendingCountAfterBatchCarrier = PendingCountAfterBatch;
    }
    const int32 PendingCountAfterBatch = PendingCountAfterBatchCarrier;

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
            // BB-005 pre-dispatch refresh immediately before mutation dispatch:
            // this inline game-thread persist is THE crash anchor - a hard
            // crash after this line leaves the last pre-dispatch record on disk.
            FMcpDiagnosticsSnapshot::Get().RecordPreDispatch(Req.RequestId, PendingCountAfterBatch);
            FMcpDiagnosticsSnapshot::Get().PersistCurrent();
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
