#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Foundation/McpTelemetryRegistry.h"

void FMcpConnectionManager::RecordAutomationTelemetry(
    const FString &RequestId, bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  // Only the bounded error CODE is forwarded; Message is deliberately dropped
  // because it routinely carries asset paths and object names.
  FMcpTelemetryRegistry::Get().EndRequest(
      RequestId, bSuccess ? TEXT("success") : TEXT("failure"), ErrorCode);

  ActiveRequestActions.Remove(RequestId);
}

int32 FMcpConnectionManager::GetActiveSocketCount() const {
  return ActiveSockets.Num();
}

void FMcpConnectionManager::RegisterRequestSocket(
    const FString &RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!RequestId.IsEmpty() && Socket.IsValid()) {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Add(RequestId, Socket);
  }
}

void FMcpConnectionManager::SetLogSubscription(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const bool bSubscribed) {
  if (!Socket.IsValid()) {
    return;
  }

  FScopeLock Lock(&LogSubscribersMutex);
  if (bSubscribed) {
    LogSubscriberSockets.Add(Socket.Get());
  } else {
    LogSubscriberSockets.Remove(Socket.Get());
  }
}

bool FMcpConnectionManager::HasLogSubscribers() const {
  FScopeLock Lock(&LogSubscribersMutex);
  return LogSubscriberSockets.Num() > 0;
}

void FMcpConnectionManager::StartRequestTelemetry(const FString &RequestId,
                                                  const FString &Action) {
  ActiveRequestActions.FindOrAdd(RequestId, Action.ToLower());
}
