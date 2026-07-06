#include "McpAutomationBridgeSubsystem.h"

#include "Async/Async.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

EAutomationQueueRejection UMcpAutomationBridgeSubsystem::QueueAutomationRequest(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    ERequestOrigin Origin)
{
    FPendingAutomationRequest Pending;
    Pending.RequestId = RequestId;
    Pending.Action = Action;
    Pending.Payload = Payload;
    Pending.RequestingSocket = RequestingSocket;
    Pending.Origin = Origin;

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

    TArray<FPendingAutomationRequest> LocalQueue;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (PendingAutomationRequests.Num() == 0)
        {
            return;
        }
        const int32 BatchSize =
            FMath::Min(MaxAutomationRequestsPerTick,
                       PendingAutomationRequests.Num());
        LocalQueue.Append(PendingAutomationRequests.GetData(), BatchSize);
        for (int32 Index = 0; Index < BatchSize; ++Index)
        {
            InFlightAutomationRequestIds.Add(
                PendingAutomationRequests[Index].RequestId);
        }
        PendingAutomationRequests.RemoveAt(
            0, BatchSize, MCP_DISALLOW_SHRINKING);
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
        {
            FScopeLock ExecutionLock(&AutomationRequestExecutionMutex);
            ProcessAutomationRequest(
                Req.RequestId,
                Req.Action,
                Req.Payload,
                Req.RequestingSocket,
                Req.Origin);
        }
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            ActiveAutomationRequestIds.Remove(Req.RequestId);
            InFlightAutomationRequestIds.Remove(Req.RequestId);
            CanceledAutomationRequestIds.Remove(Req.RequestId);
        }
    }
}
