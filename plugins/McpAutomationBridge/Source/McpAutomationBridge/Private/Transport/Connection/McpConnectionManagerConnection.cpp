#include "Transport/Connection/McpConnectionManagerPrivate.h"

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
      ServerSocket->ConnectedDelegate.AddSP(this, &FMcpConnectionManager::HandleConnected);
      ServerSocket->ClientConnectedDelegate.AddSP(this, &FMcpConnectionManager::HandleClientConnected);
      ServerSocket->ConnectionErrorDelegate.AddSP(this, &FMcpConnectionManager::HandleServerConnectionError);

      if (!ActiveSockets.Contains(ServerSocket))
        ActiveSockets.Add(ServerSocket);
      ServerSocket->Listen();
    }
  }

}

void FMcpConnectionManager::ForceReconnect(const FString &Reason,
                                           float ReconnectDelayOverride) {
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("ForceReconnect: %s"),
         *Reason);

  ForgetAllSockets();

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
