#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Async/Async.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"

namespace {
void PersistSnapshotAsync() {
  AsyncTask(ENamedThreads::GameThread,
            []() { FMcpDiagnosticsSnapshot::Get().PersistCurrent(); });
}
}  // namespace

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

  // Bind delegates to this manager instance
  // Since we are using TSharedFromThis, we can use AsShared() for binding
  // However, TFunction/Lambda binding with weak pointers is safer for async
  // callbacks

  TWeakPtr<FMcpConnectionManager> WeakSelf = AsShared();

  ClientSocket->OnMessage().AddLambda(
      [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock, const FString &Msg) {
        if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin())
          StrongSelf->HandleMessage(Sock, Msg);
      });

  ClientSocket->OnClosed().AddLambda(
      [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock, int32 Code,
                 const FString &Reason, bool bClean) {
        if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin())
          StrongSelf->HandleClosed(Sock, Code, Reason, bClean);
      });

  TWeakPtr<FMcpBridgeWebSocket> WeakSocket = ClientSocket;
  ClientSocket->OnConnectionError().AddLambda(
      [WeakSelf, WeakSocket](const FString &Error) {
        if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
          StrongSelf->HandleConnectionError(WeakSocket.Pin(), Error);
        }
      });

  ClientSocket->OnHeartbeat().AddLambda(
      [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock) {
        if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin())
          StrongSelf->HandleHeartbeat(Sock);
      });

  if (!ActiveSockets.Contains(ClientSocket)) {
    ActiveSockets.Add(ClientSocket);
  }
  bBridgeAvailable = true;

  if (ClientSocket.IsValid()) {
    ClientSocket->NotifyMessageHandlerRegistered();
  }
}

void FMcpConnectionManager::HandleConnectionError(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Error) {
  const int32 Port = Socket.IsValid() ? Socket->GetPort() : -1;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("Automation bridge socket error (port=%d): %s"), Port, *Error);
  // BB-005: an error close is a disconnect summary (memory-only on the socket
  // thread); the disk write is deferred to the game thread.
  FMcpDiagnosticsSnapshot::Get().RecordDisconnect(TEXT("error"));
  PersistSnapshotAsync();

  if (Socket.IsValid()) {
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
    Socket->OnMessage().RemoveAll(this);
    Socket->OnClosed().RemoveAll(this);
    Socket->OnConnectionError().RemoveAll(this);
    Socket->OnHeartbeat().RemoveAll(this);
    Socket->Close();
    ActiveSockets.Remove(Socket);
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
  // NF-5: 4004 (HANDSHAKE_REQUIRED) and 4005 (INVALID_CAPABILITY_TOKEN) are
  // both handshake failures; a 4005-only record left lastHandshake unset for
  // a 4004-only interaction. The close-code mapping is bounded by the store's
  // allowlist {closed, error}: 1000/1001 -> closed, everything else -> error.
  if (StatusCode == 4004 || StatusCode == 4005) {
    FMcpDiagnosticsSnapshot::Get().RecordHandshake(false);
  }
  // BB-005: a disconnect summary is memory-only on the socket thread; the disk
  // write is deferred to the game thread (H6).
  FMcpDiagnosticsSnapshot::Get().RecordDisconnect((StatusCode == 1000 || StatusCode == 1001) ? TEXT("closed") : TEXT("error"));
  PersistSnapshotAsync();
  if (Socket.IsValid()) {
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
