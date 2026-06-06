#include "McpConnectionManagerPrivate.h"

void FMcpConnectionManager::AttemptConnection() {
  if (!bBridgeAvailable)
    return;

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("AttemptConnection invoked."));

  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  if (!Settings)
    return;

  auto IsAnyServerListening = [&]() -> bool {
    for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
      if (Sock.IsValid() && Sock->IsListening())
        return true;
    }
    return false;
  };

  const bool bShouldListen = Settings->bAlwaysListen;
  if (bShouldListen && !IsAnyServerListening()) {
    const FString PortsStr = EnvListenPorts.IsEmpty() ? Settings->ListenPorts : EnvListenPorts;
    TArray<FString> PortTokens;
    if (!PortsStr.IsEmpty()) {
      PortsStr.ParseIntoArray(PortTokens, TEXT(","), true);
    }
    if (PortTokens.Num() == 0)
      PortTokens.Add(TEXT("8090"));
    if (!Settings->bMultiListen && PortTokens.Num() > 0)
      PortTokens.SetNum(1);

    const FString HostToBind =
        EnvListenHost.IsEmpty() ? Settings->ListenHost : EnvListenHost;
    TWeakPtr<FMcpConnectionManager> WeakSelf = AsShared();

    for (const FString &Token : PortTokens) {
      const FString Trimmed = Token.TrimStartAndEnd();
      if (Trimmed.IsEmpty())
        continue;

      int32 Port = 0;
      if (!LexTryParseString(Port, *Trimmed) || Port <= 0 || Port > 65535)
        continue;

      bool bAlready = false;
      for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
        if (Sock.IsValid() && Sock->IsListening() && Sock->GetPort() == Port) {
          bAlready = true;
          break;
        }
      }
      if (bAlready)
        continue;

      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("AttemptConnection: creating server listener on %s:%d"),
             *HostToBind, Port);

      TSharedPtr<FMcpBridgeWebSocket> ServerSocket =
          MakeShared<FMcpBridgeWebSocket>(Port, HostToBind,
                                          Settings->ListenBacklog,
                                          Settings->AcceptSleepSeconds,
                                          bEnableTls, TlsCertificatePath,
                                          TlsPrivateKeyPath);
      ServerSocket->InitializeWeakSelf(ServerSocket);

      ServerSocket->OnConnected().AddLambda(
          [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleConnected(Sock);
            }
          });

      ServerSocket->OnClientConnected().AddLambda(
          [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> ClientSock) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleClientConnected(ClientSock);
            }
          });

      ServerSocket->OnConnectionError().AddLambda(
          [WeakSelf](const FString &Err) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleServerConnectionError(Err);
            }
          });

      if (!ActiveSockets.Contains(ServerSocket))
        ActiveSockets.Add(ServerSocket);
      ServerSocket->Listen();
    }
  }

  if (!EndpointUrl.IsEmpty()) {
    bool bHasClientForEndpoint = false;
    for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
      if (Sock.IsValid() && !Sock->IsListening() &&
          Sock->GetPort() == ClientPort) {
        bHasClientForEndpoint = true;
        break;
      }
    }

    if (!bHasClientForEndpoint) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("AttemptConnection: creating client socket to %s"),
             *EndpointUrl);
      TMap<FString, FString> Headers;
      if (!CapabilityToken.IsEmpty()) {
        Headers.Add(TEXT("X-MCP-Capability-Token"), CapabilityToken);
      }
      TSharedPtr<FMcpBridgeWebSocket> ClientSocket =
          MakeShared<FMcpBridgeWebSocket>(EndpointUrl, TEXT("mcp-automation"),
                                          Headers, bEnableTls,
                                          TlsCertificatePath,
                                          TlsPrivateKeyPath);
      ClientSocket->InitializeWeakSelf(ClientSocket);

      TWeakPtr<FMcpConnectionManager> WeakSelf = AsShared();

      ClientSocket->OnConnected().AddLambda(
          [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleConnected(Sock);
            }
          });
      ClientSocket->OnConnectionError().AddLambda(
          [WeakSelf, ClientSocket](const FString &Err) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleConnectionError(ClientSocket, Err);
            }
          });
      ClientSocket->OnMessage().AddLambda(
          [WeakSelf](TSharedPtr<FMcpBridgeWebSocket> Sock,
                     const FString &Message) {
            if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
              StrongSelf->HandleMessage(Sock, Message);
            }
          });

      ActiveSockets.Add(ClientSocket);
      ClientSocket->Connect();
    }
  }
}

void FMcpConnectionManager::ForceReconnect(const FString &Reason,
                                           float ReconnectDelayOverride) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("ForceReconnect: %s"),
         *Reason);

  for (TSharedPtr<FMcpBridgeWebSocket> &Socket : ActiveSockets) {
    if (Socket.IsValid()) {
      Socket->Close();
    }
  }
  ActiveSockets.Empty();
  AuthenticatedSockets.Empty();
  {
    FScopeLock Lock(&RateLimitMutex);
    SocketRateLimits.Empty();
  }
  {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Empty();
  }

  bBridgeAvailable = false;
  if (bReconnectEnabled) {
    TimeUntilReconnect = (ReconnectDelayOverride >= 0.0f)
                             ? ReconnectDelayOverride
                             : AutoReconnectDelaySeconds;
    // Re-enable bridge availability flag so tick will attempt connection after
    // delay
    bBridgeAvailable = true;
  }
}
