#include "McpConnectionManager.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Reuse the log category from the subsystem for consistency
// (It is declared extern in McpAutomationBridgeSubsystem.h)

static inline FString SanitizeForLogConnMgr(const FString &In) {
  if (In.IsEmpty())
    return FString();
  FString Out;
  Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C >= 32 && C != 127)
      Out.AppendChar(C);
    else
      Out.AppendChar('?');
  }
  if (Out.Len() > 512)
    Out = Out.Left(512) + TEXT("[TRUNCATED]");
  return Out;
}

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
    const FString PortsStr =
        bEnvListenPortsSet ? EnvListenPorts : Settings->ListenPorts;
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
      if (!LexTryParseString(Port, *Trimmed) || Port <= 0)
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
  AuthenticatedSockets.Remove(ClientSocket.Get());
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

  if (Socket.IsValid()) {
    AuthenticatedSockets.Remove(Socket.Get());
    {
      FScopeLock Lock(&RateLimitMutex);
      SocketRateLimits.Remove(Socket.Get());
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
  if (Socket.IsValid()) {
    AuthenticatedSockets.Remove(Socket.Get());
    {
      FScopeLock Lock(&RateLimitMutex);
      SocketRateLimits.Remove(Socket.Get());
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

void FMcpConnectionManager::HandleMessage(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &Message) {
  if (!Socket.IsValid())
    return;
  FMcpBridgeWebSocket *SocketPtr = Socket.Get();
  FString RateLimitReason;
  if (!UpdateRateLimit(SocketPtr, true, false, RateLimitReason)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Rate limit exceeded for incoming messages: %s"),
           *RateLimitReason);
    TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
    Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
    Err->SetStringField(TEXT("error"), TEXT("RATE_LIMIT_EXCEEDED"));
    Err->SetStringField(TEXT("message"), RateLimitReason);
    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Err, Writer);
    if (Socket.IsValid() && Socket->IsConnected()) {
      Socket->Send(Serialized);
      Socket->Close(4008, TEXT("Rate limit exceeded"));
    }
    return;
  }

  TSharedPtr<FJsonObject> RootObj;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
  if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to parse incoming automation message JSON: %s"),
           *SanitizeForLogConnMgr(Message));
    return;
  }

  FString Type;
  if (!RootObj->TryGetStringField(TEXT("type"), Type)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Incoming message missing 'type' field: %s"),
           *SanitizeForLogConnMgr(Message));
    return;
  }

  if (Type.Equals(TEXT("automation_request"), ESearchCase::IgnoreCase)) {
    if (!UpdateRateLimit(SocketPtr, false, true, RateLimitReason)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Rate limit exceeded for automation requests: %s"),
             *RateLimitReason);
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("RATE_LIMIT_EXCEEDED"));
      Err->SetStringField(TEXT("message"), RateLimitReason);
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4008, TEXT("Rate limit exceeded"));
      }
      return;
    }

    FString RequestId;
    FString Action;
    RootObj->TryGetStringField(TEXT("requestId"), RequestId);
    RootObj->TryGetStringField(TEXT("action"), Action);
    TSharedPtr<FJsonObject> Payload = nullptr;
    const TSharedPtr<FJsonValue> *PayloadVal =
        RootObj->Values.Find(TEXT("payload"));
    if (PayloadVal && (*PayloadVal)->Type == EJson::Object) {
      Payload = (*PayloadVal)->AsObject();
    } else if (PayloadVal) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request payload must be a JSON object."));
      return;
    }

    if (RequestId.IsEmpty() || Action.IsEmpty()) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request missing requestId or action: %s"),
             *SanitizeForLogConnMgr(Message));
      return;
    }

    if (RequestId.Len() > 128 || Action.Len() > 128) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("automation_request fields exceed expected size."));
      return;
    }

    if (!SocketPtr || !AuthenticatedSockets.Contains(SocketPtr)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Automation request received before bridge_hello handshake."));
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("HANDSHAKE_REQUIRED"));
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4004, TEXT("Handshake required"));
      }
      return;
    }

    // Skip logging for console_command - Unreal already logs the command
    const bool bSkipLogging = Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

    // Log incoming request: action + filtered payload (exclude type/requestId)
    if (!bSkipLogging) {
      FString PayloadPreview;
      if (Payload.IsValid()) {
        TArray<FString> Parts;
        for (auto& Pair : Payload->Values) {
          if (Pair.Key != TEXT("type") && Pair.Key != TEXT("requestId")) {
            FString Val;
            if (Pair.Value->Type == EJson::String) {
              Val = FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString().Left(50));
            } else if (Pair.Value->Type == EJson::Boolean) {
              Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
            } else if (Pair.Value->Type == EJson::Number) {
              Val = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
            } else {
              Val = TEXT("...");
            }
            Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *Val));
          }
        }
        PayloadPreview = Parts.Num() > 0 ? FString::Join(Parts, TEXT(" ")) : TEXT("{}");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Request: %s %s"),
             *Action,
             *PayloadPreview.Left(200));
    }

    // Map request to socket for response routing
    {
      FScopeLock Lock(&PendingRequestsMutex);
      PendingRequestsToSockets.Add(RequestId, Socket);
    }

    // Dispatch to subsystem via callback
    if (OnMessageReceived.IsBound()) {
      OnMessageReceived.Execute(RequestId, Action, Payload, Socket);
    }
    return;
  }

  if (Type.Equals(TEXT("bridge_hello"), ESearchCase::IgnoreCase)) {
    FString ReceivedToken;
    RootObj->TryGetStringField(TEXT("capabilityToken"), ReceivedToken);
    if (bRequireCapabilityToken &&
        (ReceivedToken.IsEmpty() || ReceivedToken != CapabilityToken)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Capability token mismatch."));
      if (SocketPtr) {
        AuthenticatedSockets.Remove(SocketPtr);
      }
      TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
      Err->SetStringField(TEXT("error"), TEXT("INVALID_CAPABILITY_TOKEN"));
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer =
          TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Err, Writer);
      if (Socket.IsValid() && Socket->IsConnected()) {
        Socket->Send(Serialized);
        Socket->Close(4005, TEXT("Invalid capability token"));
      }
      return;
    }

    if (SocketPtr) {
      AuthenticatedSockets.Add(SocketPtr);
    }

    TSharedRef<FJsonObject> Ack = MakeShared<FJsonObject>();
    Ack->SetStringField(TEXT("type"), TEXT("bridge_ack"));
    Ack->SetStringField(TEXT("message"), TEXT("Automation bridge ready"));
    Ack->SetStringField(TEXT("serverName"), !ServerName.IsEmpty()
                                                ? ServerName
                                                : TEXT("UnrealEditor"));
    Ack->SetStringField(TEXT("serverVersion"), !ServerVersion.IsEmpty()
                                                   ? ServerVersion
                                                   : TEXT("unreal-engine"));

    if (ActiveSessionId.IsEmpty())
      ActiveSessionId = FGuid::NewGuid().ToString();
    Ack->SetStringField(TEXT("sessionId"), ActiveSessionId);
    Ack->SetNumberField(TEXT("protocolVersion"), 1);

    TArray<TSharedPtr<FJsonValue>> SupportedOps;
    SupportedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_request")));
    Ack->SetArrayField(TEXT("supportedOpcodes"), SupportedOps);

    TArray<TSharedPtr<FJsonValue>> ExpectedOps;
    ExpectedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_response")));
    Ack->SetArrayField(TEXT("expectedResponseOpcodes"), ExpectedOps);

    TArray<TSharedPtr<FJsonValue>> Caps;
    Caps.Add(MakeShared<FJsonValueString>(TEXT("console_commands")));
    Caps.Add(MakeShared<FJsonValueString>(TEXT("native_plugin")));
    Ack->SetArrayField(TEXT("capabilities"), Caps);

    Ack->SetNumberField(TEXT("heartbeatIntervalMs"), 0);

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Ack, Writer);
    Socket->Send(Serialized);
  }
}

bool FMcpConnectionManager::UpdateRateLimit(FMcpBridgeWebSocket* SocketPtr,
                                           bool bIncrementMessage,
                                           bool bIncrementAutomation,
                                           FString& OutReason) {
  if (!SocketPtr) {
    return true;
  }

  if (MaxMessagesPerMinute <= 0 && MaxAutomationRequestsPerMinute <= 0) {
    return true;
  }

  FScopeLock Lock(&RateLimitMutex);

  const double NowSeconds = FPlatformTime::Seconds();
  FSocketRateState& State = SocketRateLimits.FindOrAdd(SocketPtr);
  if (State.WindowStartSeconds <= 0.0) {
    State.WindowStartSeconds = NowSeconds;
  }

  if ((NowSeconds - State.WindowStartSeconds) >= 60.0) {
    State.WindowStartSeconds = NowSeconds;
    State.MessageCount = 0;
    State.AutomationRequestCount = 0;
  }

  if (bIncrementMessage) {
    ++State.MessageCount;
  }
  if (bIncrementAutomation) {
    ++State.AutomationRequestCount;
  }

  if (MaxMessagesPerMinute > 0 && State.MessageCount >= MaxMessagesPerMinute) {
    OutReason = FString::Printf(TEXT("message rate %d/%d per minute"),
                                State.MessageCount, MaxMessagesPerMinute);
    return false;
  }

  if (bIncrementAutomation && MaxAutomationRequestsPerMinute > 0 &&
      State.AutomationRequestCount >= MaxAutomationRequestsPerMinute) {
    OutReason = FString::Printf(TEXT("automation request rate %d/%d per minute"),
                                State.AutomationRequestCount,
                                MaxAutomationRequestsPerMinute);
    return false;
  }

  return true;
}

bool FMcpConnectionManager::SendRawMessage(const FString &Message) {
  if (Message.IsEmpty())
    return false;
  bool bSent = false;
  for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
    if (!Sock.IsValid() || !Sock->IsConnected())
      continue;
    if (Sock->Send(Message)) {
      bSent = true;
      break;
    }
  }
  return bSent;
}

void FMcpConnectionManager::SendControlMessage(
    const TSharedPtr<FJsonObject> &Message) {
  if (!Message.IsValid())
    return;
  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);
  SendRawMessage(Serialized);
}

void FMcpConnectionManager::SendAutomationResponse(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString &RequestId,
    bool bSuccess, const FString &Message,
    const TSharedPtr<FJsonObject> &Result, const FString &ErrorCode) {
  TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
  Response->SetStringField(TEXT("type"), TEXT("automation_response"));
  Response->SetStringField(TEXT("requestId"), RequestId);
  Response->SetBoolField(TEXT("success"), bSuccess);
  if (!Message.IsEmpty())
    Response->SetStringField(TEXT("message"), Message);
  if (!ErrorCode.IsEmpty())
    Response->SetStringField(TEXT("error"), ErrorCode);
  if (Result.IsValid())
    Response->SetObjectField(TEXT("result"), Result.ToSharedRef());

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Response, Writer);

  // Get action from telemetry for better logging context
  FString ActionName = TEXT("unknown");
  if (FAutomationRequestTelemetry* Entry = ActiveRequestTelemetry.Find(RequestId)) {
    ActionName = Entry->Action;
  }

  // Skip logging for console_command - Unreal already logs the command
  const bool bSkipLogging = ActionName.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);

  // Log result with actual values for verification
  if (!bSkipLogging) {
    FString ResultPreview;
    if (Result.IsValid() && Result->Values.Num() > 0) {
      TArray<FString> Parts;
      for (auto& Pair : Result->Values) {
        FString Val;
        if (Pair.Value->Type == EJson::String) {
          Val = FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString().Left(40));
        } else if (Pair.Value->Type == EJson::Boolean) {
          Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
        } else if (Pair.Value->Type == EJson::Number) {
          Val = FString::Printf(TEXT("%g"), Pair.Value->AsNumber());
        } else if (Pair.Value->Type == EJson::Array) {
          Val = FString::Printf(TEXT("[%d]"), Pair.Value->AsArray().Num());
        } else if (Pair.Value->Type == EJson::Object) {
          Val = TEXT("{...}");
        } else {
          Val = TEXT("?");
        }
        Parts.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *Val));
      }
      ResultPreview = FString::Printf(TEXT(" (%s)"), *FString::Join(Parts, TEXT(" ")));
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Response: %s %s%s%s"),
           *ActionName,
           bSuccess ? TEXT("OK") : TEXT("FAILED"),
           !Message.IsEmpty() ? *FString::Printf(TEXT(" \"%s\""), *Message.Left(80)) : TEXT(""),
           *ResultPreview);
  }

  RecordAutomationTelemetry(RequestId, bSuccess, Message, ErrorCode);

  bool bSent = false;
  TArray<FString> AttemptDetails;
  const int MaxAttempts = 3;

  TSharedPtr<FMcpBridgeWebSocket> MappedSocket;
  {
    FScopeLock Lock(&PendingRequestsMutex);
    if (TSharedPtr<FMcpBridgeWebSocket> *Found =
            PendingRequestsToSockets.Find(RequestId)) {
      MappedSocket = *Found;
    }
  }

  for (int Attempt = 1; Attempt <= MaxAttempts && !bSent; ++Attempt) {
    if (TargetSocket.IsValid() && TargetSocket->IsConnected()) {
      if (TargetSocket->Send(Serialized)) {
        bSent = true;
        break;
      }
    }

    if (!bSent && MappedSocket.IsValid() && MappedSocket->IsConnected()) {
      if (MappedSocket->Send(Serialized)) {
        bSent = true;
        break;
      }
    }

    if (!bSent) {
      for (const TSharedPtr<FMcpBridgeWebSocket> &Sock : ActiveSockets) {
        if (!Sock.IsValid() || !Sock->IsConnected())
          continue;
        if (Sock == TargetSocket)
          continue;
        if (MappedSocket == Sock)
          continue;
        if (Sock->Send(Serialized)) {
          bSent = true;
          break;
        }
      }
    }
  }

  if (!bSent) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Failed to deliver automation_response for RequestId=%s"),
           *RequestId);

    TSharedPtr<FJsonObject> FallbackEvent = MakeShared<FJsonObject>();
    FallbackEvent->SetStringField(TEXT("type"), TEXT("automation_event"));
    FallbackEvent->SetStringField(TEXT("event"), TEXT("response_fallback"));
    FallbackEvent->SetStringField(TEXT("requestId"), RequestId);

    TSharedPtr<FJsonObject> EventResult = MakeShared<FJsonObject>();
    EventResult->SetBoolField(TEXT("success"), bSuccess);
    if (!Message.IsEmpty())
      EventResult->SetStringField(TEXT("message"), Message);
    if (!ErrorCode.IsEmpty())
      EventResult->SetStringField(TEXT("error"), ErrorCode);
    if (Result.IsValid())
      EventResult->SetObjectField(TEXT("payload"), Result.ToSharedRef());
    FallbackEvent->SetObjectField(TEXT("result"), EventResult);

    SendControlMessage(FallbackEvent);
  }

  {
    FScopeLock Lock(&PendingRequestsMutex);
    PendingRequestsToSockets.Remove(RequestId);
  }
}

void FMcpConnectionManager::SendProgressUpdate(
    const FString& RequestId, float Percent, const FString& Message, bool bStillWorking) {
  TSharedRef<FJsonObject> Update = MakeShared<FJsonObject>();
  Update->SetStringField(TEXT("type"), TEXT("progress_update"));
  Update->SetStringField(TEXT("requestId"), RequestId);
  
  if (Percent >= 0.0f) {
    Update->SetNumberField(TEXT("percent"), Percent);
  }
  
  if (!Message.IsEmpty()) {
    Update->SetStringField(TEXT("message"), Message);
  }
  
  Update->SetBoolField(TEXT("stillWorking"), bStillWorking);
  
  // Add timestamp in ISO format
  const FDateTime Now = FDateTime::UtcNow();
  const FString Timestamp = FString::Printf(TEXT("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ"),
    Now.GetYear(), Now.GetMonth(), Now.GetDay(),
    Now.GetHour(), Now.GetMinute(), Now.GetSecond(),
    Now.GetMillisecond());
  Update->SetStringField(TEXT("timestamp"), Timestamp);
  
  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer =
      TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Update, Writer);
  
  // Find the socket for this request and send the progress update
  TSharedPtr<FMcpBridgeWebSocket> TargetSocket;
  {
    FScopeLock Lock(&PendingRequestsMutex);
    if (TSharedPtr<FMcpBridgeWebSocket>* Found = PendingRequestsToSockets.Find(RequestId)) {
      TargetSocket = *Found;
    }
  }
  
  if (TargetSocket.IsValid() && TargetSocket->IsConnected()) {
    if (!TargetSocket->Send(Serialized)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Failed to send progress update for RequestId=%s"),
             *RequestId);
    } else {
      // Verbose logging only for progress updates to avoid flooding logs
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Progress update for %s: %.1f%% %s"),
             *RequestId, Percent, *Message.Left(40));
    }
  }
}

void FMcpConnectionManager::RecordAutomationTelemetry(
    const FString &RequestId, bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  const double NowSeconds = FPlatformTime::Seconds();

  FAutomationRequestTelemetry Entry;
  if (!ActiveRequestTelemetry.RemoveAndCopyValue(RequestId, Entry)) {
    return;
  }

  const FString ActionKey =
      Entry.Action.IsEmpty() ? TEXT("unknown") : Entry.Action;
  FAutomationActionStats &Stats =
      AutomationActionTelemetry.FindOrAdd(ActionKey);

  const double DurationSeconds =
      FMath::Max(0.0, NowSeconds - Entry.StartTimeSeconds);
  if (bSuccess) {
    ++Stats.SuccessCount;
    Stats.TotalSuccessDurationSeconds += DurationSeconds;
  } else {
    ++Stats.FailureCount;
    Stats.TotalFailureDurationSeconds += DurationSeconds;
  }

  Stats.LastDurationSeconds = DurationSeconds;
  Stats.LastUpdatedSeconds = NowSeconds;
}

void FMcpConnectionManager::EmitAutomationTelemetrySummaryIfNeeded(
    double NowSeconds) {
  if (TelemetrySummaryIntervalSeconds <= 0.0)
    return;
  if ((NowSeconds - LastTelemetrySummaryLogSeconds) <
      TelemetrySummaryIntervalSeconds)
    return;

  LastTelemetrySummaryLogSeconds = NowSeconds;
  if (AutomationActionTelemetry.Num() == 0)
    return;

  TArray<FString> Lines;
  Lines.Reserve(AutomationActionTelemetry.Num());

  for (const TPair<FString, FAutomationActionStats> &Pair :
       AutomationActionTelemetry) {
    const FString &ActionKey = Pair.Key;
    const FAutomationActionStats &Stats = Pair.Value;
    const double AvgSuccess =
        Stats.SuccessCount > 0
            ? (Stats.TotalSuccessDurationSeconds / Stats.SuccessCount)
            : 0.0;
    const double AvgFailure =
        Stats.FailureCount > 0
            ? (Stats.TotalFailureDurationSeconds / Stats.FailureCount)
            : 0.0;
    Lines.Add(FString::Printf(TEXT("%s success=%d failure=%d last=%.3fs "
                                   "avgSuccess=%.3fs avgFailure=%.3fs"),
                              *ActionKey, Stats.SuccessCount,
                              Stats.FailureCount, Stats.LastDurationSeconds,
                              AvgSuccess, AvgFailure));
  }
  Lines.Sort();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Automation action telemetry summary (%d actions):\n%s"),
         Lines.Num(), *FString::Join(Lines, TEXT("\n")));
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

void FMcpConnectionManager::StartRequestTelemetry(const FString &RequestId,
                                                  const FString &Action) {
  if (!ActiveRequestTelemetry.Contains(RequestId)) {
    FAutomationRequestTelemetry Entry;
    // Store lowercase action for consistent aggregation, similar to original
    // logic
    const FString LowerAction = Action.ToLower();
    Entry.Action = LowerAction.IsEmpty() ? Action : LowerAction;
    Entry.StartTimeSeconds = FPlatformTime::Seconds();
    ActiveRequestTelemetry.Add(RequestId, Entry);
  }
}
