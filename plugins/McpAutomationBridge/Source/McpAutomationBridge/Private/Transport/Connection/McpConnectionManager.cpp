// UBT requires X.cpp to include X.h first. The private umbrella below already
// pulls it in, but the check cannot see through the umbrella, so naming it here
// keeps the build output free of a spurious per-build error.
#include "McpConnectionManager.h"

#include "Transport/Connection/McpConnectionManagerPrivate.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersCapabilityToken.h"

FMcpConnectionManager::FMcpConnectionManager() {}

FMcpConnectionManager::~FMcpConnectionManager() { Stop(); }

void FMcpConnectionManager::Initialize(
    const UMcpAutomationBridgeSettings *Settings) {
  if (Settings) {
    if (!Settings->ListenHost.IsEmpty())
      EnvListenHost = Settings->ListenHost;
    if (!Settings->ListenPorts.IsEmpty())
      EnvListenPorts = Settings->ListenPorts;
    // Route through the capability-token store so auto-generation and token-file
    // persistence are handled in one place, consistently for both transports.
    CapabilityToken = McpCapabilityTokenStore::ResolveEffectiveToken(Settings);
    if (Settings->AutoReconnectDelay > 0.0f)
      AutoReconnectDelaySeconds = Settings->AutoReconnectDelay;
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
  const auto ApplyEnvLimit = [](const TCHAR *Name, int32 &Target) {
    int32 ParsedValue = 0;
    if (LexTryParseString(ParsedValue, *FPlatformMisc::GetEnvironmentVariable(Name))) {
      Target = ParsedValue;
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Rate limit override from env: %s=%d"), Name, Target);
    }
  };
  ApplyEnvLimit(TEXT("MCP_MAX_MESSAGES_PER_MINUTE"), MaxMessagesPerMinute);
  ApplyEnvLimit(TEXT("MCP_MAX_AUTOMATION_REQUESTS_PER_MINUTE"), MaxAutomationRequestsPerMinute);
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
  SubscribeContentRootChanges();
}

void FMcpConnectionManager::Stop() {
  UnsubscribeContentRootChanges();
  if (TickerHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    TickerHandle = FTSTicker::FDelegateHandle();
  }

  bBridgeAvailable = false;
  bReconnectEnabled = false;
  TimeUntilReconnect = 0.0f;

  // Unbind first so closing the sockets below reaches no handler of a stopped manager.
  for (TSharedPtr<FMcpBridgeWebSocket> &Socket : ActiveSockets) {
    if (Socket.IsValid()) {
      Socket->ConnectedDelegate.RemoveAll(this);
      Socket->ConnectionErrorDelegate.RemoveAll(this);
      Socket->ClosedDelegate.RemoveAll(this);
      Socket->MessageDelegate.RemoveAll(this);
      Socket->HeartbeatDelegate.RemoveAll(this);
    }
  }
  ForgetAllSockets();

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("MCP connection manager stopped."));
}

void FMcpConnectionManager::SetOnMessageReceived(
    FMcpMessageReceivedCallback InCallback) {
  OnMessageReceived = InCallback;
}

bool FMcpConnectionManager::Tick(float DeltaTime) {
  FlushContentRootsUpdate();

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

  return true;
}
