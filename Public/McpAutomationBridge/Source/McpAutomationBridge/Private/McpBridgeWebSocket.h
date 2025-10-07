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

DECLARE_MULTICAST_DELEGATE(FMcpBridgeWebSocketConnectedEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketConnectionErrorEvent, const FString& /*Error*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMcpBridgeWebSocketClosedEvent, int32 /*StatusCode*/, const FString& /*Reason*/, bool /*bWasClean*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FMcpBridgeWebSocketMessageEvent, const FString& /*Message*/);

/**
 * Minimal WebSocket client used by the MCP Automation Bridge subsystem.
 * Supports text frames over unsecured ws:// transports for local automation traffic.
 */
class FMcpBridgeWebSocket final : public TSharedFromThis<FMcpBridgeWebSocket>, public FRunnable
{
public:
    FMcpBridgeWebSocket(const FString& InUrl, const FString& InProtocols, const TMap<FString, FString>& InHeaders);
    virtual ~FMcpBridgeWebSocket() override;

    void Connect();
    void Close(int32 StatusCode = 1000, const FString& Reason = FString());
    bool Send(const FString& Data);
    bool Send(const void* Data, SIZE_T Length);
    bool IsConnected() const;

    FMcpBridgeWebSocketConnectedEvent& OnConnected() { return ConnectedDelegate; }
    FMcpBridgeWebSocketConnectionErrorEvent& OnConnectionError() { return ConnectionErrorDelegate; }
    FMcpBridgeWebSocketClosedEvent& OnClosed() { return ClosedDelegate; }
    FMcpBridgeWebSocketMessageEvent& OnMessage() { return MessageDelegate; }

    // FRunnable
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

private:
    void TearDown(const FString& Reason, bool bWasClean, int32 StatusCode);
    bool PerformHandshake();
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
    FString Protocols;
    TMap<FString, FString> Headers;

    TAtomic<bool> bConnected{false};
    TAtomic<bool> bStopping{false};

    FMcpBridgeWebSocketConnectedEvent ConnectedDelegate;
    FMcpBridgeWebSocketConnectionErrorEvent ConnectionErrorDelegate;
    FMcpBridgeWebSocketClosedEvent ClosedDelegate;
    FMcpBridgeWebSocketMessageEvent MessageDelegate;

    FSocket* Socket = nullptr;
    FRunnableThread* Thread = nullptr;
    FEvent* StopEvent = nullptr;

    FCriticalSection SendMutex;
    FCriticalSection ReceiveMutex;

    FString HandshakeKey;
    FString HandshakePath;
    FString HostHeader;
    int32 Port = 80;

    TArray<uint8> PendingReceived;
    TArray<uint8> FragmentAccumulator;
    bool bFragmentMessageActive = false;
};
