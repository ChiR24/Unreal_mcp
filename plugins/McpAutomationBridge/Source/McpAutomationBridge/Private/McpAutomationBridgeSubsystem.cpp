// Ensure the subsystem type and bridge socket types are available
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSettings.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

// Define the subsystem log category declared in the public header.
DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

// Sanitize incoming text for logging: replace control characters with
// '?' and truncate long messages so logs remain readable and do not
// attempt to render unprintable glyphs in the editor which can spam
// Slate font warnings.
static inline FString SanitizeForLog(const FString& In)
{
    if (In.IsEmpty()) return FString();
    FString Out; Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
    for (int32 i = 0; i < In.Len(); ++i)
    {
        const TCHAR C = In[i];
        if (C >= 32 && C != 127) Out.AppendChar(C);
        else Out.AppendChar('?');
    }
    if (Out.Len() > 512) Out = Out.Left(512) + TEXT("[TRUNCATED]");
    return Out;
}

void UMcpAutomationBridgeSubsystem::AttemptConnection()
{
    if (!bBridgeAvailable)
    {
        return;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("AttemptConnection invoked: ensuring listeners/clients based on settings."));

    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    if (!Settings)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("AttemptConnection: unable to read settings; aborting connection attempts."));
        return;
    }

    // Helper: detect whether we already have any listening server sockets
    auto IsAnyServerListening = [&]() -> bool
    {
        for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
        {
            if (Sock.IsValid() && Sock->IsListening()) return true;
        }
        return false;
    };

    // Start server/listen sockets when configured
    const bool bShouldListen = Settings->bAlwaysListen;
    if (bShouldListen && !IsAnyServerListening())
    {
        const FString PortsStr = bEnvListenPortsSet ? EnvListenPorts : Settings->ListenPorts;
        TArray<FString> PortTokens;
        if (!PortsStr.IsEmpty())
        {
            PortsStr.ParseIntoArray(PortTokens, TEXT(","), true);
        }

        // Fallback to default port if nothing provided
        if (PortTokens.Num() == 0)
        {
            PortTokens.Add(TEXT("8090"));
        }

        // Respect multi-listen toggle
        if (!Settings->bMultiListen && PortTokens.Num() > 0)
        {
            PortTokens.SetNum(1);
        }

        const FString HostToBind = EnvListenHost.IsEmpty() ? Settings->ListenHost : EnvListenHost;
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);

        for (const FString& Token : PortTokens)
        {
            const FString Trimmed = Token.TrimStartAndEnd();
            if (Trimmed.IsEmpty()) continue;

            int32 Port = 0;
            if (!LexTryParseString(Port, *Trimmed) || Port <= 0)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("AttemptConnection: invalid listen port token '%s'"), *Trimmed);
                continue;
            }

            // Skip if already have a listening socket on this port
            bool bAlready = false;
            for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
            {
                if (Sock.IsValid() && Sock->IsListening() && Sock->GetPort() == Port)
                {
                    bAlready = true; break;
                }
            }
            if (bAlready) continue;

            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("AttemptConnection: creating server listener on %s:%d"), *HostToBind, Port);

            TSharedPtr<FMcpBridgeWebSocket> ServerSocket = MakeShared<FMcpBridgeWebSocket>(Port, HostToBind, Settings->ListenBacklog, Settings->AcceptSleepSeconds);
            ServerSocket->InitializeWeakSelf(ServerSocket);

            // When the server socket reports it's ready (bListening), the connected delegate will fire.
            ServerSocket->OnConnected().AddLambda([WeakThis](TSharedPtr<FMcpBridgeWebSocket> Sock)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleConnected(Sock);
                }
            });

            // When a new client completes handshake, the server will broadcast ClientConnected
            ServerSocket->OnClientConnected().AddLambda([WeakThis](TSharedPtr<FMcpBridgeWebSocket> ClientSock)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleClientConnected(ClientSock);
                }
            });

            // Report server-level listen/connect failures
            ServerSocket->OnConnectionError().AddLambda([WeakThis](const FString& Err)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleServerConnectionError(Err);
                }
            });

            // Store and start listening
            if (!ActiveSockets.Contains(ServerSocket)) ActiveSockets.Add(ServerSocket);
            ServerSocket->Listen();
        }
    }

    // Optionally connect back to an endpoint (client mode)
    if (!EndpointUrl.IsEmpty())
    {
        // Do not create duplicate client-mode sockets for the same endpoint
        bool bHasClientForEndpoint = false;
        for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
        {
            if (Sock.IsValid() && !Sock->IsListening() && Sock->GetPort() == ClientPort)
            {
                bHasClientForEndpoint = true; break;
            }
        }

        if (!bHasClientForEndpoint)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("AttemptConnection: creating client socket to %s"), *EndpointUrl);
            TMap<FString, FString> Headers;
            if (!CapabilityToken.IsEmpty())
            {
                Headers.Add(TEXT("X-MCP-Capability-Token"), CapabilityToken);
            }
            TSharedPtr<FMcpBridgeWebSocket> ClientSocket = MakeShared<FMcpBridgeWebSocket>(EndpointUrl, TEXT("mcp-automation"), Headers);
            ClientSocket->InitializeWeakSelf(ClientSocket);

            TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);

            ClientSocket->OnConnected().AddLambda([WeakThis](TSharedPtr<FMcpBridgeWebSocket> Sock)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleConnected(Sock);
                }
            });
            ClientSocket->OnConnectionError().AddLambda([WeakThis, ClientSocket](const FString& Err)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleConnectionError(ClientSocket, Err);
                }
            });
            ClientSocket->OnMessage().AddLambda([WeakThis](TSharedPtr<FMcpBridgeWebSocket> Sock, const FString& Message)
            {
                if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
                {
                    Pinned->HandleMessage(Sock, Message);
                }
            });

            ActiveSockets.Add(ClientSocket);
            ClientSocket->Connect();
        }
    }
}

// The in-file implementation of ProcessAutomationRequest was intentionally
// removed from this translation unit. The function is now implemented in
// McpAutomationBridge_ProcessRequest.cpp to avoid duplicate definitions and
// to keep this file focused. See that file for the full request dispatcher
// and per-action handlers.

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode)
{
    if (!TargetSocket.IsValid() || !TargetSocket->IsConnected())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Requesting socket for RequestId=%s is not connected; will attempt to deliver via mapped or alternate sockets."), *RequestId);
        // Do not return here — attempt mapped socket or other active sockets below.
    }

    TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("type"), TEXT("automation_response"));
    Response->SetStringField(TEXT("requestId"), RequestId);
    Response->SetBoolField(TEXT("success"), bSuccess);
    if (!Message.IsEmpty())
    {
        Response->SetStringField(TEXT("message"), Message);
    }
    if (!ErrorCode.IsEmpty())
    {
        Response->SetStringField(TEXT("error"), ErrorCode);
    }
    if (Result.IsValid())
    {
        Response->SetObjectField(TEXT("result"), Result.ToSharedRef());
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Response, Writer);

    // Optional per-socket telemetry (disabled by default). When enabled the
    // subsystem will emit a Log-level delivery summary and per-socket details
    // to aid debugging intermittent delivery failures.
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    const bool bSocketTelemetryEnabled = Settings && Settings->bEnableSocketTelemetry;

    RecordAutomationTelemetry(RequestId, bSuccess, Message, ErrorCode);

    bool bSent = false;
    TArray<FString> AttemptDetails;
    const int MaxAttempts = 3;
    // Locate any socket mapped to this RequestId (if present) so we can
    // attempt delivery even if the immediate TargetSocket is not valid.
    TSharedPtr<FMcpBridgeWebSocket>* Mapped = PendingRequestsToSockets.Find(RequestId);
    for (int Attempt = 1; Attempt <= MaxAttempts && !bSent; ++Attempt)
    {
        // Try the original requesting socket first
        if (TargetSocket.IsValid() && TargetSocket->IsConnected())
        {
            const int32 TargetPortCandidate = TargetSocket->GetPort();
            const bool bSockSent = TargetSocket->Send(Serialized);
            // Record attempt details for optional telemetry and keep per-socket
            // verbose traces at VeryVerbose so normal logs aren't flooded.
            AttemptDetails.Add(FString::Printf(TEXT("Attempt %d -> requesting socket=%p port=%d connected=%s listening=%s sent=%s bytes=%d"), Attempt, (void*)TargetSocket.Get(), TargetPortCandidate, TargetSocket->IsConnected() ? TEXT("true") : TEXT("false"), TargetSocket->IsListening() ? TEXT("true") : TEXT("false"), bSockSent ? TEXT("ok") : TEXT("failed"), Serialized.Len()));
            UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Attempt %d: send automation_response RequestId=%s to requesting socket (port=%d): %s (bytes=%d)"), Attempt, *RequestId, TargetPortCandidate, bSockSent ? TEXT("ok") : TEXT("failed"), Serialized.Len());
            if (bSockSent)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("automation_response RequestId=%s delivered to requesting socket."), *RequestId);
                bSent = true;
                break;
            }
        }

        // Try mapping stored socket
        if (!bSent && Mapped && Mapped->IsValid() && (*Mapped)->IsConnected())
        {
            const int32 MappedPortCandidate = (*Mapped)->GetPort();
            const bool bSockSent = (*Mapped)->Send(Serialized);
            AttemptDetails.Add(FString::Printf(TEXT("Attempt %d -> mapped socket=%p port=%d connected=%s listening=%s sent=%s bytes=%d"), Attempt, (void*)(*Mapped).Get(), MappedPortCandidate, (*Mapped)->IsConnected() ? TEXT("true") : TEXT("false"), (*Mapped)->IsListening() ? TEXT("true") : TEXT("false"), bSockSent ? TEXT("ok") : TEXT("failed"), Serialized.Len()));
            UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Attempt %d: send automation_response RequestId=%s to mapped socket (port=%d): %s (bytes=%d)"), Attempt, *RequestId, MappedPortCandidate, bSockSent ? TEXT("ok") : TEXT("failed"), Serialized.Len());
            if (bSockSent)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("automation_response RequestId=%s delivered to mapped socket."), *RequestId);
                bSent = true;
                break;
            }
        }

        // Try other active sockets as best-effort
        if (!bSent)
        {
            for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
            {
                if (!Sock.IsValid() || !Sock->IsConnected()) continue;
                // Skip already attempted sockets
                if (Sock == TargetSocket) continue;
                if (Mapped && *Mapped == Sock) continue;
                const int32 AltPort = Sock->GetPort();
                    const bool bAltSent = Sock->Send(Serialized);
                    AttemptDetails.Add(FString::Printf(TEXT("Attempt %d -> alt socket=%p port=%d connected=%s listening=%s sent=%s bytes=%d"), Attempt, (void*)Sock.Get(), AltPort, Sock->IsConnected() ? TEXT("true") : TEXT("false"), Sock->IsListening() ? TEXT("true") : TEXT("false"), bAltSent ? TEXT("ok") : TEXT("failed"), Serialized.Len()));
                    if (bAltSent)
                    {
                        bSent = true;
                        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Attempt %d: sent automation_response RequestId=%s via alternate socket (port=%d, bytes=%d)."), Attempt, *RequestId, AltPort, Serialized.Len());
                        break;
                    }
            }
        }

        // If not sent and not last attempt, try again on next iteration.
        if (!bSent && Attempt < MaxAttempts)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Attempt %d failed to deliver automation_response for RequestId=%s; retrying..."), Attempt, *RequestId);
        }
    }

        if (!bSent)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Failed to deliver automation_response for RequestId=%s to any connected socket (activeSockets=%d)."), *RequestId, ActiveSockets.Num());

            // If socket telemetry is enabled, emit the per-attempt details so
            // operators can correlate failures with socket lifecycle state.
            if (bSocketTelemetryEnabled && AttemptDetails.Num() > 0)
            {
                const FString Joined = FString::Join(AttemptDetails, TEXT("\n"));
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("automation_response RequestId=%s delivery attempts:\n%s"), *RequestId, *Joined);
            }

            // As a robust fallback, broadcast an automation_event that contains
            // the original requestId and a small result object describing the
            // response. The Node automation bridge will accept such events and
            // synthesize an automation_response to resolve pending promises.
            TSharedPtr<FJsonObject> FallbackEvent = MakeShared<FJsonObject>();
            FallbackEvent->SetStringField(TEXT("type"), TEXT("automation_event"));
            FallbackEvent->SetStringField(TEXT("event"), TEXT("response_fallback"));
            FallbackEvent->SetStringField(TEXT("requestId"), RequestId);

            TSharedPtr<FJsonObject> EventResult = MakeShared<FJsonObject>();
            EventResult->SetBoolField(TEXT("success"), bSuccess);
            if (!Message.IsEmpty()) EventResult->SetStringField(TEXT("message"), Message);
            if (!ErrorCode.IsEmpty()) EventResult->SetStringField(TEXT("error"), ErrorCode);
            // If there is a structured result payload, include it under 'payload'
            // so consumers can inspect more detailed fields when available.
            if (Result.IsValid())
            {
                EventResult->SetObjectField(TEXT("payload"), Result.ToSharedRef());
            }
            FallbackEvent->SetObjectField(TEXT("result"), EventResult);

            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("automation_response RequestId=%s could not be delivered; broadcasting fallback automation_event to all sockets (activeSockets=%d)."), *RequestId, ActiveSockets.Num());
            SendControlMessage(FallbackEvent);
        }

        // Clean up the request tracking regardless to avoid stale mappings
        PendingRequestsToSockets.Remove(RequestId);
}

void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(const FString& RequestId, const bool bSuccess, const FString& Message, const FString& ErrorCode)
{
    const double NowSeconds = FPlatformTime::Seconds();
    EmitAutomationTelemetrySummaryIfNeeded(NowSeconds);

    FAutomationRequestTelemetry Entry;
    if (!ActiveRequestTelemetry.RemoveAndCopyValue(RequestId, Entry))
    {
        return;
    }

    const FString ActionKey = Entry.Action.IsEmpty() ? TEXT("unknown") : Entry.Action;
    FAutomationActionStats& Stats = AutomationActionTelemetry.FindOrAdd(ActionKey);

    const double DurationSeconds = FMath::Max(0.0, NowSeconds - Entry.StartTimeSeconds);
    if (bSuccess)
    {
        ++Stats.SuccessCount;
        Stats.TotalSuccessDurationSeconds += DurationSeconds;
    }
    else
    {
        ++Stats.FailureCount;
        Stats.TotalFailureDurationSeconds += DurationSeconds;
    }

    Stats.LastDurationSeconds = DurationSeconds;
    Stats.LastUpdatedSeconds = NowSeconds;

    const FString SanitizedMsg = SanitizeForLog(Message);
    const FString SanitizedErr = SanitizeForLog(ErrorCode);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Automation telemetry action=%s result=%s duration=%.3fs message=%s error=%s"), *ActionKey, bSuccess ? TEXT("success") : TEXT("failure"), DurationSeconds, *SanitizedMsg, *SanitizedErr);
}

void UMcpAutomationBridgeSubsystem::EmitAutomationTelemetrySummaryIfNeeded(const double NowSeconds)
{
    if (TelemetrySummaryIntervalSeconds <= 0.0)
    {
        return;
    }

    if ((NowSeconds - LastTelemetrySummaryLogSeconds) < TelemetrySummaryIntervalSeconds)
    {
        return;
    }

    LastTelemetrySummaryLogSeconds = NowSeconds;

    if (AutomationActionTelemetry.Num() == 0)
    {
        return;
    }

    TArray<FString> Lines;
    Lines.Reserve(AutomationActionTelemetry.Num());

    for (const TPair<FString, FAutomationActionStats>& Pair : AutomationActionTelemetry)
    {
        const FString& ActionKey = Pair.Key;
        const FAutomationActionStats& Stats = Pair.Value;
        const double AvgSuccess = Stats.SuccessCount > 0 ? (Stats.TotalSuccessDurationSeconds / Stats.SuccessCount) : 0.0;
        const double AvgFailure = Stats.FailureCount > 0 ? (Stats.TotalFailureDurationSeconds / Stats.FailureCount) : 0.0;
        Lines.Add(FString::Printf(TEXT("%s success=%d failure=%d last=%.3fs avgSuccess=%.3fs avgFailure=%.3fs"), *ActionKey, Stats.SuccessCount, Stats.FailureCount, Stats.LastDurationSeconds, AvgSuccess, AvgFailure));
    }

    Lines.Sort();

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation action telemetry summary (%d actions):\n%s"), Lines.Num(), *FString::Join(Lines, TEXT("\n")));
}

void UMcpAutomationBridgeSubsystem::SendAutomationError(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, const FString& Message, const FString& ErrorCode)
{
    const FString ResolvedError = ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation request failed (%s): %s"), *ResolvedError, *SanitizeForLog(Message));
    SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr, ResolvedError);
}

void UMcpAutomationBridgeSubsystem::StartBridge()
{
    if (!TickerHandle.IsValid())
    {
        const FTickerDelegate TickDelegate = FTickerDelegate::CreateUObject(this, &UMcpAutomationBridgeSubsystem::Tick);
        const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
        const float Interval = (Settings && Settings->TickerIntervalSeconds > 0.0f) ? Settings->TickerIntervalSeconds : 0.25f;
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate, Interval);
    }
    // Mark the bridge as available so AttemptConnection() will run.
    bBridgeAvailable = true;
    bReconnectEnabled = AutoReconnectDelaySeconds > 0.0f;
    TimeUntilReconnect = 0.0f;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Starting MCP automation bridge."));
    AttemptConnection();
}

void UMcpAutomationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("McpAutomationBridgeSubsystem initializing."));

    // Read settings once at startup
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    if (Settings)
    {
        if (!Settings->ListenHost.IsEmpty()) EnvListenHost = Settings->ListenHost;
        if (!Settings->ListenPorts.IsEmpty()) EnvListenPorts = Settings->ListenPorts;
        if (!Settings->EndpointUrl.IsEmpty()) EndpointUrl = Settings->EndpointUrl;
        if (!Settings->CapabilityToken.IsEmpty()) CapabilityToken = Settings->CapabilityToken;
        if (Settings->AutoReconnectDelay > 0.0f) AutoReconnectDelaySeconds = Settings->AutoReconnectDelay;
        if (Settings->ClientPort > 0) ClientPort = Settings->ClientPort;
        bRequireCapabilityToken = Settings->bRequireCapabilityToken;
        if (Settings->HeartbeatTimeoutSeconds > 0.0f) HeartbeatTimeoutSeconds = Settings->HeartbeatTimeoutSeconds;
    }

    // Start the bridge services (ticker, sockets) on initialization
    StartBridge();
}

void UMcpAutomationBridgeSubsystem::Deinitialize()
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("McpAutomationBridgeSubsystem deinitializing."));
    StopBridge();
    Super::Deinitialize();
}

bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString& Message)
{
    if (Message.IsEmpty()) return false;
    bool bSent = false;
    for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : ActiveSockets)
    {
        if (!Sock.IsValid() || !Sock->IsConnected()) continue;
        if (Sock->Send(Message))
        {
            bSent = true;
            break;
        }
    }
    if (!bSent)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("SendRawMessage: no connected sockets to send message."));
    }
    return bSent;
}

bool UMcpAutomationBridgeSubsystem::Tick(float DeltaTime)
{
    // Handle reconnect countdown
    if (bReconnectEnabled && TimeUntilReconnect > 0.0f)
    {
        TimeUntilReconnect -= DeltaTime;
        if (TimeUntilReconnect <= 0.0f)
        {
            TimeUntilReconnect = 0.0f;
            if (bBridgeAvailable)
            {
                AttemptConnection();
            }
        }
    }

    // Heartbeat monitoring
    if (bHeartbeatTrackingEnabled && HeartbeatTimeoutSeconds > 0.0f && LastHeartbeatTimestamp > 0.0)
    {
        const double Now = FPlatformTime::Seconds();
        if ((Now - LastHeartbeatTimestamp) > HeartbeatTimeoutSeconds)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Heartbeat timed out; forcing reconnect."));
            ForceReconnect(TEXT("Heartbeat timeout"));
        }
    }

    // Always continue ticking while the subsystem is alive
    return true;
}

void UMcpAutomationBridgeSubsystem::StopBridge()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle = FTSTicker::FDelegateHandle();
    }

    BridgeState = EMcpAutomationBridgeState::Disconnected;
    bBridgeAvailable = false;
    bReconnectEnabled = false;
    TimeUntilReconnect = 0.0f;

    // Close all active sockets
    for (TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid())
        {
            Socket->OnConnected().RemoveAll(this);
            Socket->OnConnectionError().RemoveAll(this);
            Socket->OnClosed().RemoveAll(this);
            Socket->OnMessage().RemoveAll(this);
            Socket->OnHeartbeat().RemoveAll(this);
            Socket->Close();
        }
    }
    ActiveSockets.Empty();
    PendingRequestsToSockets.Empty();

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge stopped."));
}

void UMcpAutomationBridgeSubsystem::HandleConnected(TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Socket.IsValid()) return;
    const int32 Port = Socket->GetPort();
    if (Socket->IsListening())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge listening on port=%d (host=%s)"), Port, *Socket->GetListenHost());
    }
    else if (Socket->IsConnected())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Automation bridge connected (socket port=%d)."), Port);
    }

    // Mark the bridge as connected/available when any socket becomes ready
    bBridgeAvailable = true;
    BridgeState = EMcpAutomationBridgeState::Connected;
}

void UMcpAutomationBridgeSubsystem::HandleClientConnected(TSharedPtr<FMcpBridgeWebSocket> ClientSocket)
{
    if (!ClientSocket.IsValid()) return;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Client socket connected (port=%d)"), ClientSocket->GetPort());

    // Wire up per-client delegates so we can receive messages and closures
    ClientSocket->OnMessage().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleMessage);
    ClientSocket->OnClosed().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleClosed);

    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
    TWeakPtr<FMcpBridgeWebSocket> WeakSocket = ClientSocket;
    ClientSocket->OnConnectionError().AddLambda([WeakThis, WeakSocket](const FString& Error)
    {
        if (UMcpAutomationBridgeSubsystem* StrongThis = WeakThis.Get())
        {
            StrongThis->HandleConnectionError(WeakSocket.Pin(), Error);
        }
    });

    ClientSocket->OnHeartbeat().AddUObject(this, &UMcpAutomationBridgeSubsystem::HandleHeartbeat);

    // Avoid adding the same socket pointer more than once — duplicates
    // cause repeated broadcast attempts and noisy logs. Use pointer
    // identity to deduplicate.
    if (!ActiveSockets.Contains(ClientSocket))
    {
        ActiveSockets.Add(ClientSocket);
    }
    bBridgeAvailable = true;
    BridgeState = EMcpAutomationBridgeState::Connected;

    // Signal the worker thread that the game-thread message handler has been
    // registered so any initial handshake frames sent right after the
    // upgrade won't be lost due to a race. This is a no-op for client-mode
    // sockets that initiated the connection.
    if (ClientSocket.IsValid())
    {
        ClientSocket->NotifyMessageHandlerRegistered();
    }
}

void UMcpAutomationBridgeSubsystem::HandleConnectionError(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Error)
{
    const int32 Port = Socket.IsValid() ? Socket->GetPort() : -1;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge socket error (port=%d): %s"), Port, *Error);

    if (Socket.IsValid())
    {
        Socket->OnMessage().RemoveAll(this);
        Socket->OnClosed().RemoveAll(this);
        Socket->OnConnectionError().RemoveAll(this);
        Socket->OnHeartbeat().RemoveAll(this);
        Socket->Close();
        ActiveSockets.Remove(Socket);
    }

    if (ActiveSockets.Num() == 0)
    {
        BridgeState = EMcpAutomationBridgeState::Disconnected;
        bBridgeAvailable = false;
        if (bReconnectEnabled)
        {
            TimeUntilReconnect = AutoReconnectDelaySeconds;
        }
    }
}

void UMcpAutomationBridgeSubsystem::HandleServerConnectionError(const FString& Error)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Automation bridge server error: %s"), *Error);
    // When server-level failures occur, mark disconnected and schedule reconnect
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    bBridgeAvailable = false;
    if (bReconnectEnabled)
    {
        TimeUntilReconnect = AutoReconnectDelaySeconds;
    }
}

void UMcpAutomationBridgeSubsystem::HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket, int32 StatusCode, const FString& Reason, bool bWasClean)
{
    const int32 Port = Socket.IsValid() ? Socket->GetPort() : -1;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Socket closed: port=%d code=%d reason=%s clean=%s"), Port, StatusCode, *Reason, bWasClean ? TEXT("true") : TEXT("false"));
    if (Socket.IsValid())
    {
        ActiveSockets.Remove(Socket);
    }
    if (ActiveSockets.Num() == 0)
    {
        BridgeState = EMcpAutomationBridgeState::Disconnected;
        bBridgeAvailable = false;
        if (bReconnectEnabled)
        {
            TimeUntilReconnect = AutoReconnectDelaySeconds;
        }
    }
}

void UMcpAutomationBridgeSubsystem::HandleMessage(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Message)
{
    if (!Socket.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Received message from invalid socket; ignoring."));
        return;
    }

    // Parse JSON and dispatch automation_request messages to the main dispatcher
    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Failed to parse incoming automation message JSON: %s"), *SanitizeForLog(Message));
        return;
    }

    FString Type;
    if (!RootObj->TryGetStringField(TEXT("type"), Type))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Incoming message missing 'type' field: %s"), *SanitizeForLog(Message));
        return;
    }

    if (Type.Equals(TEXT("automation_request"), ESearchCase::IgnoreCase))
    {
        FString RequestId;
        FString Action;
        RootObj->TryGetStringField(TEXT("requestId"), RequestId);
        RootObj->TryGetStringField(TEXT("action"), Action);
        TSharedPtr<FJsonObject> Payload = nullptr;
        const TSharedPtr<FJsonValue>* PayloadVal = RootObj->Values.Find(TEXT("payload"));
        if (PayloadVal && (*PayloadVal)->Type == EJson::Object)
        {
            Payload = (*PayloadVal)->AsObject();
        }

        if (RequestId.IsEmpty() || Action.IsEmpty())
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("automation_request missing requestId or action: %s"), *SanitizeForLog(Message));
            return;
        }

        ProcessAutomationRequest(RequestId, Action, Payload, Socket);
        return;
    }

    // Handle bridge handshake and heartbeat/ping messages here so that the
    // Node automation client and the plugin can negotiate a session and
    // capabilities. These control messages are lightweight and should not be
    // forwarded to the general automation request dispatcher.
    if (Type.Equals(TEXT("bridge_hello"), ESearchCase::IgnoreCase))
    {
        // Optionally validate capability token
        FString ReceivedToken;
        RootObj->TryGetStringField(TEXT("capabilityToken"), ReceivedToken);
        if (bRequireCapabilityToken && (ReceivedToken.IsEmpty() || ReceivedToken != CapabilityToken))
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Automation bridge capability token mismatch; closing connection. Received token=%s, expected=%s"), *ReceivedToken, *CapabilityToken);
            TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("type"), TEXT("bridge_error"));
            Err->SetStringField(TEXT("error"), TEXT("INVALID_CAPABILITY_TOKEN"));
            FString Serialized;
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
            FJsonSerializer::Serialize(Err, Writer);
            if (Socket.IsValid() && Socket->IsConnected())
            {
                Socket->Send(Serialized);
                Socket->Close(4005, TEXT("Invalid capability token"));
            }
            return;
        }

        // Build the bridge_ack payload
        TSharedRef<FJsonObject> Ack = MakeShared<FJsonObject>();
        Ack->SetStringField(TEXT("type"), TEXT("bridge_ack"));
        Ack->SetStringField(TEXT("message"), TEXT("Automation bridge ready"));

        if (!ServerName.IsEmpty())
        {
            Ack->SetStringField(TEXT("serverName"), ServerName);
        }
        else
        {
            Ack->SetStringField(TEXT("serverName"), TEXT("UnrealEditor"));
        }

        if (!ServerVersion.IsEmpty())
        {
            Ack->SetStringField(TEXT("serverVersion"), ServerVersion);
        }
        else
        {
            Ack->SetStringField(TEXT("serverVersion"), TEXT("unreal-engine"));
        }

        if (ActiveSessionId.IsEmpty())
        {
            ActiveSessionId = FGuid::NewGuid().ToString();
        }
        Ack->SetStringField(TEXT("sessionId"), ActiveSessionId);
        Ack->SetNumberField(TEXT("protocolVersion"), 1);

        // supported/expected opcodes
        TArray<TSharedPtr<FJsonValue>> SupportedOps;
        SupportedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_request")));
        Ack->SetArrayField(TEXT("supportedOpcodes"), SupportedOps);

        TArray<TSharedPtr<FJsonValue>> ExpectedOps;
        ExpectedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_response")));
        Ack->SetArrayField(TEXT("expectedResponseOpcodes"), ExpectedOps);

        // Capabilities: advertise console_commands and plugin-native handlers
        TArray<TSharedPtr<FJsonValue>> Caps;
        Caps.Add(MakeShared<FJsonValueString>(TEXT("console_commands")));
        Caps.Add(MakeShared<FJsonValueString>(TEXT("native_plugin")));
        Ack->SetArrayField(TEXT("capabilities"), Caps);

        // Heartbeat/limits
        Ack->SetNumberField(TEXT("heartbeatIntervalMs"), 0);
        Ack->SetNumberField(TEXT("maxPendingRequests"), 32);

        // Supported protocols / ports
        TArray<TSharedPtr<FJsonValue>> SupportedProtos;
        SupportedProtos.Add(MakeShared<FJsonValueString>(TEXT("mcp-automation")));
        Ack->SetArrayField(TEXT("supportedProtocols"), SupportedProtos);

        TArray<TSharedPtr<FJsonValue>> AvailablePorts;
        for (const TSharedPtr<FMcpBridgeWebSocket>& S : ActiveSockets)
        {
            if (S.IsValid())
            {
                AvailablePorts.Add(MakeShared<FJsonValueNumber>(S->GetPort()));
            }
        }
        Ack->SetArrayField(TEXT("availablePorts"), AvailablePorts);
        Ack->SetNumberField(TEXT("activePort"), Socket.IsValid() ? Socket->GetPort() : 0);

        // Serialize and send ack
        FString AckSerialized;
        const TSharedRef<TJsonWriter<>> AckWriter = TJsonWriterFactory<>::Create(&AckSerialized);
        FJsonSerializer::Serialize(Ack, AckWriter);
        if (Socket.IsValid() && Socket->IsConnected())
        {
            const bool bSent = Socket->Send(AckSerialized);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Sent bridge_ack to socket(port=%d): %s (bytes=%d)"), Socket->GetPort(), bSent ? TEXT("ok") : TEXT("failed"), AckSerialized.Len());
        }
        else
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Unable to send bridge_ack: socket invalid or disconnected."));
        }
        return;
    }

    if (Type.Equals(TEXT("bridge_ping"), ESearchCase::IgnoreCase))
    {
        // Reply with bridge_pong including session id and optional nonce
        TSharedRef<FJsonObject> Pong = MakeShared<FJsonObject>();
        Pong->SetStringField(TEXT("type"), TEXT("bridge_pong"));
        Pong->SetStringField(TEXT("sessionId"), ActiveSessionId);
        // Copy nonce if present
        FString Nonce;
        if (RootObj->TryGetStringField(TEXT("nonce"), Nonce) && !Nonce.IsEmpty())
        {
            Pong->SetStringField(TEXT("nonce"), Nonce);
        }
        FString PongSerialized;
        const TSharedRef<TJsonWriter<>> PongWriter = TJsonWriterFactory<>::Create(&PongSerialized);
        FJsonSerializer::Serialize(Pong, PongWriter);
        if (Socket.IsValid() && Socket->IsConnected())
        {
            Socket->Send(PongSerialized);
        }
        return;
    }

    // Other message types can be handled here (automation_response, automation_event)
}

void UMcpAutomationBridgeSubsystem::HandleHeartbeat(TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    RecordHeartbeat();
}

void UMcpAutomationBridgeSubsystem::RecordHeartbeat()
{
    LastHeartbeatTimestamp = FPlatformTime::Seconds();
}

void UMcpAutomationBridgeSubsystem::ResetHeartbeatTracking()
{
    LastHeartbeatTimestamp = 0.0;
    HeartbeatTimeoutSeconds = 0.0f;
    bHeartbeatTrackingEnabled = false;
}

void UMcpAutomationBridgeSubsystem::ForceReconnect(const FString& Reason, float ReconnectDelayOverride)
{
    const float EffectiveDelay = ReconnectDelayOverride >= 0.0f ? ReconnectDelayOverride : AutoReconnectDelaySeconds;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Forcing automation bridge reconnect (delay %.2f s): %s"), EffectiveDelay, Reason.IsEmpty() ? TEXT("no reason provided") : *Reason);

    // Close all active sockets
    for (TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (Socket.IsValid())
        {
            Socket->OnConnected().RemoveAll(this);
            Socket->OnConnectionError().RemoveAll(this);
            Socket->OnClosed().RemoveAll(this);
            Socket->OnMessage().RemoveAll(this);
            Socket->OnHeartbeat().RemoveAll(this);
            Socket->Close();
        }
    }
    ActiveSockets.Empty();
    PendingRequestsToSockets.Empty();

    ResetHeartbeatTracking();
    BridgeState = EMcpAutomationBridgeState::Disconnected;
    bReconnectEnabled = true;
    TimeUntilReconnect = EffectiveDelay;

    if (EffectiveDelay <= 0.0f && bBridgeAvailable)
    {
        AttemptConnection();
    }
}

void UMcpAutomationBridgeSubsystem::SendControlMessage(const TSharedPtr<FJsonObject>& Message)
{
    if (!Message.IsValid())
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Ignoring control message send; payload invalid."));
        return;
    }

    FString Serialized;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Message.ToSharedRef(), Writer);

    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Outbound control message: %s"), *Serialized);

    // Send control message to all connected sockets. Log per-socket send
    // success so tests can determine whether broadcasts reached clients.
    int32 SentCount = 0;
    int32 FailedCount = 0;
    // Optional per-socket telemetry (off by default)
    const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    const bool bSocketTelemetryEnabled = Settings && Settings->bEnableSocketTelemetry;
    TArray<FString> AttemptDetails;
    // Try to deliver to mapped socket for this requestId first (if present)
    FString RequestId;
    if (Message->TryGetStringField(TEXT("requestId"), RequestId) && !RequestId.IsEmpty())
    {
        TSharedPtr<FMcpBridgeWebSocket>* Mapped = PendingRequestsToSockets.Find(RequestId);
        if (Mapped && Mapped->IsValid() && (*Mapped)->IsConnected())
        {
            const bool bOk = (*Mapped)->Send(Serialized);
            if (bOk) { SentCount++; }
            else { FailedCount++; }
            // Record details for optional telemetry and emit a VeryVerbose per-socket
            // trace so routine broadcasts remain quiet under normal logging.
            AttemptDetails.Add(FString::Printf(TEXT("mapped=%p port=%d connected=%s listening=%s sent=%s bytes=%d"), (void*)(*Mapped).Get(), (*Mapped)->GetPort(), (*Mapped)->IsConnected() ? TEXT("true") : TEXT("false"), (*Mapped)->IsListening() ? TEXT("true") : TEXT("false"), bOk ? TEXT("ok") : TEXT("failed"), Serialized.Len()));
            UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Control message event=%s requestId=%s -> mapped socket(port=%d): %s (bytes=%d)"), *Message->GetStringField(TEXT("event")), *RequestId, (*Mapped)->GetPort(), bOk ? TEXT("ok") : TEXT("failed"), Serialized.Len());
        }
    }

    // Broadcast to remaining sockets (skip any pointer we've already tried)
    TSet<TSharedPtr<FMcpBridgeWebSocket>> Tried;
    if (!RequestId.IsEmpty()) { if (TSharedPtr<FMcpBridgeWebSocket>* TFound = PendingRequestsToSockets.Find(RequestId)) if (TFound && TFound->IsValid()) Tried.Add(*TFound); }

    for (const TSharedPtr<FMcpBridgeWebSocket>& Socket : ActiveSockets)
    {
        if (!Socket.IsValid()) continue;
        if (Tried.Contains(Socket)) continue;
        if (!Socket->IsConnected()) { UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Control message not sent to socket(port=%d): socket not connected."), Socket->GetPort()); continue; }
        const bool bOk = Socket->Send(Serialized);
        if (bOk) { SentCount++; }
        else { FailedCount++; }
        // Record per-socket attempt info for telemetry and emit a VeryVerbose
        // trace so tests/developers can opt-in for deeper delivery inspection.
        AttemptDetails.Add(FString::Printf(TEXT("socket=%p port=%d connected=%s listening=%s sent=%s bytes=%d"), (void*)Socket.Get(), Socket->GetPort(), Socket->IsConnected() ? TEXT("true") : TEXT("false"), Socket->IsListening() ? TEXT("true") : TEXT("false"), bOk ? TEXT("ok") : TEXT("failed"), Serialized.Len()));
        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Control message event=%s requestId=%s -> socket(port=%d): %s (bytes=%d)"), *Message->GetStringField(TEXT("event")), Message->HasField(TEXT("requestId")) ? *Message->GetStringField(TEXT("requestId")) : TEXT("n/a"), Socket->GetPort(), bOk ? TEXT("ok") : TEXT("failed"), Serialized.Len());
    }

    // Summarize broadcast result at Warning level only when nothing was
    // delivered. Partial delivery failures are demoted to Verbose by default
    // to avoid log spam; enabling socket telemetry will raise the summary to
    // Log and include per-socket details.
    if (SentCount == 0)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Control message event=%s requestId=%s could not be delivered to any socket (active=%d failed=%d)."), *Message->GetStringField(TEXT("event")), Message->HasField(TEXT("requestId")) ? *Message->GetStringField(TEXT("requestId")) : TEXT("n/a"), ActiveSockets.Num(), FailedCount);
        if (bSocketTelemetryEnabled && AttemptDetails.Num() > 0)
        {
            const FString Joined = FString::Join(AttemptDetails, TEXT("\n"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Control message event=%s requestId=%s delivery attempts:\n%s"), *Message->GetStringField(TEXT("event")), Message->HasField(TEXT("requestId")) ? *Message->GetStringField(TEXT("requestId")) : TEXT("n/a"), *Joined);
        }
    }
    else if (FailedCount > 0)
    {
        if (bSocketTelemetryEnabled)
        {
            const FString Joined = FString::Join(AttemptDetails, TEXT("\n"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Control message event=%s requestId=%s delivered to %d sockets (failed on %d). Details:\n%s"), *Message->GetStringField(TEXT("event")), Message->HasField(TEXT("requestId")) ? *Message->GetStringField(TEXT("requestId")) : TEXT("n/a"), SentCount, FailedCount, *Joined);
        }
        else
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Control message event=%s requestId=%s delivered to %d sockets (failed on %d)."), *Message->GetStringField(TEXT("event")), Message->HasField(TEXT("requestId")) ? *Message->GetStringField(TEXT("requestId")) : TEXT("n/a"), SentCount, FailedCount);
        }
    }
}

// Drain and process any automation requests that were enqueued while the
// subsystem was busy. This implementation lives in the primary subsystem
// translation unit to ensure the symbol is available at link time for
// any callsites that reference it (including scope-exit lambdas).
void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests()
{
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this]() { this->ProcessPendingAutomationRequests(); });
        return;
    }

    TArray<FPendingAutomationRequest> LocalQueue;
    {
        FScopeLock Lock(&PendingAutomationRequestsMutex);
        if (PendingAutomationRequests.Num() == 0)
        {
            bPendingRequestsScheduled = false;
            return;
        }
        LocalQueue = MoveTemp(PendingAutomationRequests);
        PendingAutomationRequests.Empty();
        bPendingRequestsScheduled = false;
    }

    for (const FPendingAutomationRequest& Req : LocalQueue)
    {
        ProcessAutomationRequest(Req.RequestId, Req.Action, Req.Payload, Req.RequestingSocket);
    }
}

