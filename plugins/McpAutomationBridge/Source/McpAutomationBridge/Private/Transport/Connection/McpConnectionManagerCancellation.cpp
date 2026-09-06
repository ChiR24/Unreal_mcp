#include "Transport/Connection/McpConnectionManagerPrivate.h"

void FMcpConnectionManager::SetOnAutomationRequestCancelled(
    FMcpRequestCancelledCallback InCallback) {
  OnAutomationRequestCancelled = MoveTemp(InCallback);
}

bool FMcpConnectionManager::IsCancelAllowedForSocket(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId) const {
  FScopeLock Lock(&PendingRequestsMutex);
  if (const TSharedPtr<FMcpBridgeWebSocket>* Owner =
          PendingRequestsToSockets.Find(RequestId)) {
    // Tracked request: only the owning socket may cancel it.
    return Owner->Get() == Socket.Get();
  }
  // Untracked (legacy/untracked) request: forward for backward compatibility.
  return true;
}

void FMcpConnectionManager::HandleCancelRequest(
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& RequestId) {
  if (RequestId.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request missing requestId: %s"),
           *McpAutomationBridgeSubsystemResponse::SanitizeForLog(RequestId));
    return;
  }

  if (RequestId.Len() > 128) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request requestId exceeds expected size."));
    return;
  }

  bool bIsAuthenticated = false;
  if (Socket.IsValid()) {
    FScopeLock Lock(&AuthSocketsMutex);
    bIsAuthenticated = AuthenticatedSockets.Contains(Socket.Get());
  }
  if (!bIsAuthenticated) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request received before bridge_hello handshake."));
    return;
  }

  // Scope cancellation to the requesting socket. PendingRequestsToSockets maps
  // each in-flight request to the socket that dispatched it (recorded in
  // McpConnectionManagerMessages.cpp and removed when the response is sent in
  // McpConnectionManagerResponses.cpp), so the entry is absent for a request
  // that already completed. Reject cancels from a socket that does not own the
  // request to stop one authenticated client cancelling another's request.
  if (!IsCancelAllowedForSocket(Socket, RequestId)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request rejected: RequestId=%s is owned by a "
                "different authenticated socket; cancelling another client's "
                "request is not permitted."),
           *RequestId);
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Cancel request received: %s"), *RequestId);

  if (OnAutomationRequestCancelled.IsBound()) {
    OnAutomationRequestCancelled.Execute(RequestId);
  }
}
