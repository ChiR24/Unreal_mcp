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

#if WITH_SSL
struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;
struct ssl_st;
typedef struct ssl_st SSL;
#endif

class FMcpBridgeWebSocket;

DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketConnectedEvent, TSharedPtr<FMcpBridgeWebSocket>);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketConnectionErrorEvent, const FString& /*Error*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FMcpBridgeWebSocketClosedEvent, TSharedPtr<FMcpBridgeWebSocket>, int32, const FString&, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMcpBridgeWebSocketMessageEvent, TSharedPtr<FMcpBridgeWebSocket>, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketHeartbeatEvent, TSharedPtr<FMcpBridgeWebSocket>);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketClientConnectedEvent, TSharedPtr<FMcpBridgeWebSocket>);

/**
 * Minimal WebSocket client/server used by the MCP Automation Bridge subsystem.
 * Supports text frames over ws:// and optional wss:// transports for local automation traffic.
 */
class FMcpBridgeWebSocket final : public TSharedFromThis<FMcpBridgeWebSocket>, public FRunnable
{
public:
    FMcpBridgeWebSocket(const FString& InUrl, const FString& InProtocols, const TMap<FString, FString>& InHeaders, bool bInEnableTls = false, const FString& InTlsCertificatePath = FString(), const FString& InTlsPrivateKeyPath = FString());
    FMcpBridgeWebSocket(int32 InPort, const FString& InHost = FString(), int32 InListenBacklog = 10, float InAcceptSleepSeconds = 0.01f, bool bInEnableTls = false, const FString& InTlsCertificatePath = FString(), const FString& InTlsPrivateKeyPath = FString());
    FMcpBridgeWebSocket(FSocket* InClientSocket, bool bInEnableTls = false, const FString& InTlsCertificatePath = FString(), const FString& InTlsPrivateKeyPath = FString());
    virtual ~FMcpBridgeWebSocket() override;

    void InitializeWeakSelf(const TSharedPtr<FMcpBridgeWebSocket>& InShared);

    void Connect();
    void Listen();
    void Close(int32 StatusCode = 1000, const FString& Reason = FString());
    bool Send(const FString& Data);
    bool Send(const void* Data, SIZE_T Length);
    bool IsConnected() const;
    bool IsListening() const;

    // Accessors for diagnostics
    FString GetListenHost() const { return ListenHost; }
    int32 GetPort() const { return Port; }

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

    // Notify the socket implementation that the message handler has been
    // registered on the game thread. This is used to avoid a race where the
    // client may send the initial application-level handshake immediately
    // after the WebSocket upgrade completes and before game-thread handlers
    // are attached. The server thread will wait briefly for this signal
    // before it begins draining frames.
    void NotifyMessageHandlerRegistered();

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
    bool SendRaw(const uint8* Data, int32 Length, int32& OutBytesSent);
    bool RecvRaw(uint8* Data, int32 Length, int32& OutBytesRead);
#if WITH_SSL
    bool InitializeTlsContext(bool bServer);
    bool EstablishTls(bool bServer);
#endif
    void ShutdownTls();
    void CloseNativeSocket();
    FSocket* DetachSocket();

    FString Url;
    FSocket* Socket;
    int32 Port;
    FString Protocols;
    TMap<FString, FString> Headers;
    FString ListenHost;

    // Server tuning (moved later to ensure proper initialization order)

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
    FCriticalSection ClientSocketsMutex;
    TArray<TSharedPtr<FMcpBridgeWebSocket>> ClientSockets;

    // Server tuning
    int32 ListenBacklog = 10;
    float AcceptSleepSeconds = 0.01f;

    // Connection state
    bool bConnected;
    bool bListening;
    bool bStopping;

    // Handshake data
    FString HostHeader;
    FString HandshakePath;
    FString HandshakeKey;

    bool bUseTls;
    bool bTlsServer;
    bool bSslInitialized;
    bool bOwnsSslContext;
#if WITH_SSL
    SSL_CTX* SslContext;
    SSL* SslHandle;
#else
    void* SslContext;
    void* SslHandle;
#endif
    UPTRINT NativeSocketHandle;
    bool bNativeSocketReleased;
    FString TlsCertificatePath;
    FString TlsPrivateKeyPath;
    
    // Synchronization event used to coordinate between the server socket
    // worker thread and the game thread. When a server-accepted connection
    // completes the HTTP/WebSocket upgrade the worker thread will wait for
    // the game thread to attach message handlers (OnMessage) so that the
    // initial application-level handshake (bridge_hello) is not lost.
    FEvent* HandlerReadyEvent;
    // Set to true by the game thread when it has registered the message
    // handler for this client connection.
    TAtomic<bool> bHandlerRegistered;
};
