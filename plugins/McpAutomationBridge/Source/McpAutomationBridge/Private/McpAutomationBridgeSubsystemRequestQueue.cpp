#include "McpAutomationBridgeSubsystem.h"

#include "Async/Async.h"

void UMcpAutomationBridgeSubsystem::QueueAutomationRequest(
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
        PendingAutomationRequests.Add(MoveTemp(Pending));
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Verbose,
        TEXT("Queued automation request for core ticker: RequestId=%s action=%s"),
        *RequestId,
        *Action);
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
        LocalQueue = MoveTemp(PendingAutomationRequests);
        PendingAutomationRequests.Empty();
    }

    for (const FPendingAutomationRequest& Req : LocalQueue)
    {
        ProcessAutomationRequest(
            Req.RequestId,
            Req.Action,
            Req.Payload,
            Req.RequestingSocket,
            Req.Origin);
    }
}
