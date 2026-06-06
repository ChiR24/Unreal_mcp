#include "McpConnectionManagerPrivate.h"

FMcpConnectionManager::FMcpConnectionManager() {}

FMcpConnectionManager::~FMcpConnectionManager() { Stop(); }

void FMcpConnectionManager::Initialize(
    const UMcpAutomationBridgeSettings *Settings) {
  if (Settings) {
    if (!Settings->ListenHost.IsEmpty())
      EnvListenHost = Settings->ListenHost;
    if (!Settings->ListenPorts.IsEmpty())
      EnvListenPorts = Settings->ListenPorts;
    if (!Settings->EndpointUrl.IsEmpty())
      EndpointUrl = Settings->EndpointUrl;
    if (!Settings->CapabilityToken.IsEmpty())
      CapabilityToken = Settings->CapabilityToken;
    if (Settings->AutoReconnectDelay > 0.0f)
      AutoReconnectDelaySeconds = Settings->AutoReconnectDelay;
    if (Settings->ClientPort > 0)
      ClientPort = Settings->ClientPort;
    bRequireCapabilityToken = Settings->bRequireCapabilityToken;
    if (Settings->HeartbeatTimeoutSeconds > 0.0f)
      HeartbeatTimeoutSeconds = Settings->HeartbeatTimeoutSeconds;
    if (Settings->MaxMessagesPerMinute >= 0)
      MaxMessagesPerMinute = Settings->MaxMessagesPerMinute;
    if (Settings->MaxAutomationRequestsPerMinute >= 0)
      MaxAutomationRequestsPerMinute = Settings->MaxAutomationRequestsPerMinute;
    bEnableTls = Settings->bEnableTls;
    if (!Settings->TlsCertificatePath.IsEmpty())
      TlsCertificatePath = Settings->TlsCertificatePath;
    if (!Settings->TlsPrivateKeyPath.IsEmpty())
      TlsPrivateKeyPath = Settings->TlsPrivateKeyPath;
  }

  // Allow environment variable overrides for rate limiting (useful for tests)
  // Set MCP_MAX_MESSAGES_PER_MINUTE=0 or MCP_MAX_AUTOMATION_REQUESTS_PER_MINUTE=0 to disable
  FString EnvMaxMessages = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_MAX_MESSAGES_PER_MINUTE"));
  if (!EnvMaxMessages.IsEmpty()) {
    int32 ParsedValue = 0;
    if (LexTryParseString(ParsedValue, *EnvMaxMessages)) {
      MaxMessagesPerMinute = ParsedValue;
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Rate limit override from env: MCP_MAX_MESSAGES_PER_MINUTE=%d"),
             MaxMessagesPerMinute);
    }
  }
  FString EnvMaxAutomation = FPlatformMisc::GetEnvironmentVariable(TEXT("MCP_MAX_AUTOMATION_REQUESTS_PER_MINUTE"));
  if (!EnvMaxAutomation.IsEmpty()) {
    int32 ParsedValue = 0;
    if (LexTryParseString(ParsedValue, *EnvMaxAutomation)) {
      MaxAutomationRequestsPerMinute = ParsedValue;
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Rate limit override from env: MCP_MAX_AUTOMATION_REQUESTS_PER_MINUTE=%d"),
             MaxAutomationRequestsPerMinute);
    }
  }
}

void FMcpConnectionManager::Start() {
  if (!TickerHandle.IsValid()) {
    // We use a lambda for the ticker that holds a weak pointer to this manager
    TWeakPtr<FMcpConnectionManager> WeakSelf = AsShared();
    const FTickerDelegate TickDelegate =
        FTickerDelegate::CreateLambda([WeakSelf](float DeltaTime) -> bool {
          if (TSharedPtr<FMcpConnectionManager> StrongSelf = WeakSelf.Pin()) {
            return StrongSelf->Tick(DeltaTime);
          }
          return false;
        });

    const UMcpAutomationBridgeSettings *Settings =
        GetDefault<UMcpAutomationBridgeSettings>();
    const float Interval = (Settings && Settings->TickerIntervalSeconds > 0.0f)
                               ? Settings->TickerIntervalSeconds
                               : 0.25f;
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, Interval);
  }

  bBridgeAvailable = true;
  bReconnectEnabled = AutoReconnectDelaySeconds > 0.0f;
  TimeUntilReconnect = 0.0f;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Starting MCP connection manager."));
  AttemptConnection();
}

void FMcpConnectionManager::Stop() {
  if (TickerHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    TickerHandle = FTSTicker::FDelegateHandle();
  }

  bBridgeAvailable = false;
  bReconnectEnabled = false;
  TimeUntilReconnect = 0.0f;

  // Close all active sockets
  for (TSharedPtr<FMcpBridgeWebSocket> &Socket : ActiveSockets) {
    if (Socket.IsValid()) {
      Socket->OnConnected().RemoveAll(this);
      Socket->OnConnectionError().RemoveAll(this);
      Socket->OnClosed().RemoveAll(this);
      Socket->OnMessage().RemoveAll(this);
      Socket->OnHeartbeat().RemoveAll(this);
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

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("MCP connection manager stopped."));
}

bool FMcpConnectionManager::IsConnected() const {
  for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
    if (Sock.IsValid() && Sock->IsConnected())
      return true;
  }
  return false;
}

void FMcpConnectionManager::SetOnMessageReceived(
    FMcpMessageReceivedCallback InCallback) {
  OnMessageReceived = InCallback;
}

bool FMcpConnectionManager::Tick(float DeltaTime) {
  // Handle reconnect countdown
  if (bReconnectEnabled && TimeUntilReconnect > 0.0f) {
    TimeUntilReconnect -= DeltaTime;
    if (TimeUntilReconnect <= 0.0f) {
      TimeUntilReconnect = 0.0f;
      if (bBridgeAvailable) {
        AttemptConnection();
      }
    }
  }

  // Heartbeat monitoring
  if (bHeartbeatTrackingEnabled && HeartbeatTimeoutSeconds > 0.0f &&
      LastHeartbeatTimestamp > 0.0) {
    const double Now = FPlatformTime::Seconds();
    if ((Now - LastHeartbeatTimestamp) > HeartbeatTimeoutSeconds) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Heartbeat timed out; forcing reconnect."));
      ForceReconnect(TEXT("Heartbeat timeout"));
    }
  }

  // Telemetry summary
  EmitAutomationTelemetrySummaryIfNeeded(FPlatformTime::Seconds());

  return true;
}
