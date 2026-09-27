#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

void FMcpConnectionManager::HandleConnected(
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!Socket.IsValid())
    return;
  const int32 Port = Socket->GetPort();
  if (Socket->IsListening()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Automation bridge listening on port=%d"), Port);
  } else if (Socket->IsConnected()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Automation bridge connected (socket port=%d)."), Port);
  }
  bBridgeAvailable = true;
}

void FMcpConnectionManager::HandleClientConnected(
    TSharedPtr<FMcpBridgeWebSocket> ClientSocket) {
  if (!ClientSocket.IsValid())
    return;
  {
    FScopeLock Lock(&AuthSocketsMutex);
    AuthenticatedSockets.Remove(ClientSocket.Get());
  }
  ForgetSocketPrincipal(ClientSocket.Get());
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Client socket connected (port=%d)"), ClientSocket->GetPort());

  // AddSP pins this manager per call; the error event carries no socket, so it rides as a weak payload.
  ClientSocket->MessageDelegate.AddSP(this, &FMcpConnectionManager::HandleMessage);
  ClientSocket->ClosedDelegate.AddSP(this, &FMcpConnectionManager::HandleClosed);
  ClientSocket->HeartbeatDelegate.AddSP(this, &FMcpConnectionManager::HandleHeartbeat);
  ClientSocket->ConnectionErrorDelegate.AddSP(this, &FMcpConnectionManager::HandleConnectionError,
                                              TWeakPtr<FMcpBridgeWebSocket>(ClientSocket));

  if (!ActiveSockets.Contains(ClientSocket)) {
    ActiveSockets.Add(ClientSocket);
  }
  bBridgeAvailable = true;

  if (ClientSocket.IsValid()) {
    ClientSocket->NotifyMessageHandlerRegistered();
  }
}

void FMcpConnectionManager::HandleConnectionError(
    const FString &Error, TWeakPtr<FMcpBridgeWebSocket> WeakSocket) {
  const TSharedPtr<FMcpBridgeWebSocket> Socket = WeakSocket.Pin();
  const int32 Port = Socket.IsValid() ? Socket->GetPort() : -1;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("Automation bridge socket error (port=%d): %s"), Port, *Error);
  // An error close is a disconnect summary (memory-only on the socket
  // thread); the disk write is deferred to the game thread.
  FMcpDiagnosticsSnapshot::Get().RecordDisconnect(TEXT("error"));
  FMcpDiagnosticsSnapshot::PersistCurrentAsync();

  if (Socket.IsValid()) {
    Socket->MessageDelegate.RemoveAll(this);
    Socket->ClosedDelegate.RemoveAll(this);
    Socket->ConnectionErrorDelegate.RemoveAll(this);
    Socket->HeartbeatDelegate.RemoveAll(this);
    Socket->Close();
    ForgetSocket(Socket);
  }

  if (ActiveSockets.Num() == 0) {
    if (bReconnectEnabled) {
      TimeUntilReconnect = AutoReconnectDelaySeconds;
    }
  }
}

void FMcpConnectionManager::HandleServerConnectionError(const FString &Error) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
         TEXT("Automation bridge server error: %s"), *Error);
  if (bReconnectEnabled) {
    TimeUntilReconnect = AutoReconnectDelaySeconds;
  }
}

void FMcpConnectionManager::HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket,
                                         int32 StatusCode,
                                         const FString &Reason,
                                         bool bWasClean) {
  const int32 Port = Socket.IsValid() ? Socket->GetPort() : -1;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Socket closed: port=%d code=%d reason=%s clean=%s"), Port,
         StatusCode, *Reason, bWasClean ? TEXT("true") : TEXT("false"));
  // 4004 (HANDSHAKE_REQUIRED) and 4005 (INVALID_CAPABILITY_TOKEN) are
  // both handshake failures; a 4005-only record left lastHandshake unset for
  // a 4004-only interaction. The close-code mapping is bounded by the store's
  // allowlist {closed, error}: 1000/1001 -> closed, everything else -> error.
  if (StatusCode == 4004 || StatusCode == 4005) {
    FMcpDiagnosticsSnapshot::Get().RecordHandshake(false);
  }
  // A disconnect summary is memory-only on the socket thread; the disk
  // write is deferred to the game thread (H6).
  FMcpDiagnosticsSnapshot::Get().RecordDisconnect((StatusCode == 1000 || StatusCode == 1001) ? TEXT("closed") : TEXT("error"));
  FMcpDiagnosticsSnapshot::PersistCurrentAsync();
  if (Socket.IsValid()) {
    ForgetSocket(Socket);
  }
  if (ActiveSockets.Num() == 0 && bReconnectEnabled) {
    TimeUntilReconnect = AutoReconnectDelaySeconds;
  }
}

void FMcpConnectionManager::HandleHeartbeat(
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  LastHeartbeatTimestamp = FPlatformTime::Seconds();
  if (!bHeartbeatTrackingEnabled) {
    bHeartbeatTrackingEnabled = true;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Heartbeat tracking enabled."));
  }
}

void FMcpConnectionManager::ForgetSocket(const TSharedPtr<FMcpBridgeWebSocket> &Socket) {
  {
    FScopeLock Lock(&AuthSocketsMutex);
    AuthenticatedSockets.Remove(Socket.Get());
  }
  ForgetSocketPrincipal(Socket.Get());
  {
    FScopeLock Lock(&LogSubscribersMutex);
    LogSubscriberSockets.Remove(Socket.Get());
  }
  {
    FScopeLock Lock(&RateLimitMutex);
    SocketRateLimits.Remove(Socket.Get());
  }
  {
    FScopeLock Lock(&PendingRequestsMutex);
    for (auto It = PendingRequestsToSockets.CreateIterator(); It; ++It) {
      if (It->Value.Get() == Socket.Get()) {
        It.RemoveCurrent();
      }
    }
  }
  ActiveSockets.Remove(Socket);
}

void FMcpConnectionManager::ForgetAllSockets() {
  for (TSharedPtr<FMcpBridgeWebSocket> &Socket : ActiveSockets) {
    if (Socket.IsValid()) {
      Socket->Close();
    }
  }
  ActiveSockets.Empty();
  {
    FScopeLock Lock(&AuthSocketsMutex);
    AuthenticatedSockets.Empty();
    // Principals are security state keyed by a raw socket pointer; they must not
    // outlive the sockets just released.
    SocketPrincipals.Empty();
  }
  {
    FScopeLock Lock(&LogSubscribersMutex);
    LogSubscriberSockets.Empty();
  }
  {
    FScopeLock Lock(&RateLimitMutex);
    SocketRateLimits.Empty();
  }
  {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Empty();
  }
}
