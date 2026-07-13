#include "Transport/Connection/McpConnectionManagerPrivate.h"

void FMcpConnectionManager::SetOnAutomationRequestCancelled(
    FMcpRequestCancelledCallback InCallback) {
  OnAutomationRequestCancelled = MoveTemp(InCallback);
}

void FMcpConnectionManager::HandleCancelRequest(
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& RequestId) {
  if (RequestId.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request missing requestId: %s"),
           *SanitizeForLogConnMgr(RequestId));
    return;
  }

  if (RequestId.Len() > 128) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request requestId exceeds expected size."));
    return;
  }

  if (!Socket.IsValid() ||
      !AuthenticatedSockets.Contains(Socket.Get())) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("cancel_request received before bridge_hello handshake."));
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Cancel request received: %s"), *RequestId);

  if (OnAutomationRequestCancelled.IsBound()) {
    OnAutomationRequestCancelled.Execute(RequestId);
  }
}
