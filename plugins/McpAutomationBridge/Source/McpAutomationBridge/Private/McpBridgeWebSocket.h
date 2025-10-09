#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"

class FSocket;
class FInternetAddr;
class FRunnableThread;
class FEvent;

class FMcpBridgeWebSocket;

DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketConnectedEvent, TSharedPtr<FMcpBridgeWebSocket>);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketConnectionErrorEvent, const FString& /*Error*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FMcpBridgeWebSocketClosedEvent, TSharedPtr<FMcpBridgeWebSocket>, int32, const FString&, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMcpBridgeWebSocketMessageEvent, TSharedPtr<FMcpBridgeWebSocket>, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketHeartbeatEvent, TSharedPtr<FMcpBridgeWebSocket>);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketClientConnectedEvent, TSharedPtr<FMcpBridgeWebSocket>);

/**
 * Minimal WebSocket client/server used by the MCP Automation Bridge subsystem.
 * Supports text frames over unsecured ws:// transports for local automation traffic.
 */
class FMcpBridgeWebSocket final : public TSharedFromThis<FMcpBridgeWebSocket>, public FRunnable
{
public:
    FMcpBridgeWebSocket(const FString& InUrl, const FString& InProtocols, const TMap<FString, FString>& InHeaders);
    FMcpBridgeWebSocket(int32 InPort);
    FMcpBridgeWebSocket(FSocket* InClientSocket);
    virtual ~FMcpBridgeWebSocket() override;

    void InitializeWeakSelf(const TSharedPtr<FMcpBridgeWebSocket>& InShared);

    void Connect();
    void Listen();
    void Close(int32 StatusCode = 1000, const FString& Reason = FString());
    bool Send(const FString& Data);
    bool Send(const void* Data, SIZE_T Length);
    bool IsConnected() const;
    bool IsListening() const;

    void SendHeartbeatPing();

    // Delegates
    FMcpBridgeWebSocketConnectedEvent ConnectedDelegate;
    FMcpBridgeWebSocketConnectionErrorEvent ConnectionErrorDelegate;
    FMcpBridgeWebSocketClosedEvent ClosedDelegate;
    FMcpBridgeWebSocketMessageEvent MessageDelegate;
    FMcpBridgeWebSocketHeartbeatEvent HeartbeatDelegate;
    FMcpBridgeWebSocketClientConnectedEvent ClientConnectedDelegate;

    // Threading
    FCriticalSection SendMutex;
    FCriticalSection ReceiveMutex;

    FMcpBridgeWebSocketConnectedEvent& OnConnected() { return ConnectedDelegate; }
    FMcpBridgeWebSocketConnectionErrorEvent& OnConnectionError() { return ConnectionErrorDelegate; }
    FMcpBridgeWebSocketClosedEvent& OnClosed() { return ClosedDelegate; }
    FMcpBridgeWebSocketMessageEvent& OnMessage() { return MessageDelegate; }
    FMcpBridgeWebSocketHeartbeatEvent& OnHeartbeat() { return HeartbeatDelegate; }
    FMcpBridgeWebSocketClientConnectedEvent& OnClientConnected() { return ClientConnectedDelegate; }

    // FRunnable
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

private:
    uint32 RunClient();
    uint32 RunServer();
    void TearDown(const FString& Reason, bool bWasClean, int32 StatusCode);
    bool PerformHandshake();
    bool PerformServerHandshake();
    bool ResolveEndpoint(TSharedPtr<FInternetAddr>& OutAddr);
    bool SendFrame(const TArray<uint8>& Frame);
    bool SendCloseFrame(int32 StatusCode, const FString& Reason);
    bool SendTextFrame(const void* Data, SIZE_T Length);
    bool SendControlFrame(uint8 ControlOpCode, const TArray<uint8>& Payload);
    void HandleTextPayload(const TArray<uint8>& Payload);
    void ResetFragmentState();
    bool ReceiveFrame();
    bool ReceiveExact(uint8* Buffer, SIZE_T Length);

    FString Url;
    FSocket* Socket;
    int32 Port;
    FString Protocols;
    TMap<FString, FString> Headers;

    TArray<uint8> PendingReceived;
    TArray<uint8> FragmentAccumulator;
    bool bFragmentMessageActive;

    TWeakPtr<FMcpBridgeWebSocket> SelfWeakPtr;

    // Server mode members
    bool bServerMode;
    bool bServerAcceptedConnection;
    FSocket* ListenSocket;
    FRunnableThread* Thread;
    FEvent* StopEvent;

    // Connection state
    bool bConnected;
    bool bListening;
    bool bStopping;

    // Handshake data
    FString HostHeader;
    FString HandshakePath;
    FString HandshakeKey;
};
