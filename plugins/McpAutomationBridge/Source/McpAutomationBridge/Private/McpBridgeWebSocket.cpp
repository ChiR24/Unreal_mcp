#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "Containers/StringConv.h"
#include "HAL/Event.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

using namespace McpBridgeWebSocket;

FMcpBridgeWebSocket::FMcpBridgeWebSocket(
    const FString &InUrl, const FString &InProtocols,
    const TMap<FString, FString> &InHeaders, bool bInEnableTls,
    const FString &InTlsCertificatePath, const FString &InTlsPrivateKeyPath)
    : Url(InUrl), Socket(nullptr), Port(0), Protocols(InProtocols),
      Headers(InHeaders), ListenHost(), PendingReceived(),
      FragmentAccumulator(), bFragmentMessageActive(false), SelfWeakPtr(),
      bServerMode(false), bServerAcceptedConnection(false),
      ListenSocket(nullptr), Thread(nullptr), StopEvent(nullptr),
      ClientSockets(), ListenBacklog(10), AcceptSleepSeconds(0.01f),
      bConnected(false), bListening(false), bStopping(false),
      bCloseStarted(false),
      bUseTls(bInEnableTls), bTlsServer(false), bSslInitialized(false),
      bOwnsSslContext(false), SslContext(nullptr), SslHandle(nullptr),
      NativeSocketHandle(0), bNativeSocketReleased(false),
      TlsCertificatePath(InTlsCertificatePath),
      TlsPrivateKeyPath(InTlsPrivateKeyPath) {
  HandlerReadyEvent = nullptr;
  bHandlerRegistered = false;
}

FMcpBridgeWebSocket::FMcpBridgeWebSocket(int32 InPort, const FString &InHost,
                                         int32 InListenBacklog,
                                         float InAcceptSleepSeconds,
                                         bool bInEnableTls,
                                         const FString &InTlsCertificatePath,
                                         const FString &InTlsPrivateKeyPath)
    : Url(), Socket(nullptr), Port(InPort), Protocols(TEXT("mcp-automation")),
      Headers(), ListenHost(InHost), PendingReceived(), FragmentAccumulator(),
      bFragmentMessageActive(false), SelfWeakPtr(), bServerMode(true),
      bServerAcceptedConnection(false), ListenSocket(nullptr), Thread(nullptr),
      StopEvent(nullptr), ClientSockets(), ListenBacklog(InListenBacklog),
      AcceptSleepSeconds(InAcceptSleepSeconds), bConnected(false),
      bListening(false), bStopping(false), bCloseStarted(false),
      bUseTls(bInEnableTls),
      bTlsServer(true), bSslInitialized(false), bOwnsSslContext(false),
      SslContext(nullptr), SslHandle(nullptr), NativeSocketHandle(0),
      bNativeSocketReleased(false), TlsCertificatePath(InTlsCertificatePath),
      TlsPrivateKeyPath(InTlsPrivateKeyPath) {
  HandlerReadyEvent = nullptr;
  bHandlerRegistered = false;
}

FMcpBridgeWebSocket::FMcpBridgeWebSocket(FSocket *InClientSocket,
                                         bool bInEnableTls,
                                         const FString &InTlsCertificatePath,
                                         const FString &InTlsPrivateKeyPath)
    : Url(), Socket(InClientSocket), Port(0), Protocols(TEXT("mcp-automation")),
      Headers(), ListenHost(), PendingReceived(), FragmentAccumulator(),
      bFragmentMessageActive(false), SelfWeakPtr(), bServerMode(false),
      bServerAcceptedConnection(true), ListenSocket(nullptr), Thread(nullptr),
      StopEvent(nullptr), ClientSockets(), ListenBacklog(10),
      AcceptSleepSeconds(0.01f), bConnected(true), bListening(false),
      bStopping(false), bCloseStarted(false), bUseTls(bInEnableTls),
      bTlsServer(true),
      bSslInitialized(false), bOwnsSslContext(false), SslContext(nullptr),
      SslHandle(nullptr), NativeSocketHandle(0), bNativeSocketReleased(false),
      TlsCertificatePath(InTlsCertificatePath),
      TlsPrivateKeyPath(InTlsPrivateKeyPath) {
  HandlerReadyEvent = nullptr;
  bHandlerRegistered = false;
}

FMcpBridgeWebSocket::~FMcpBridgeWebSocket() {
  Close();

  if (Thread) {
    Thread->Kill(true);
    delete Thread;
    Thread = nullptr;
  }

  ShutdownTls();
  if (HandlerReadyEvent) {
    FPlatformProcess::ReturnSynchEventToPool(HandlerReadyEvent);
    HandlerReadyEvent = nullptr;
  }

  if (StopEvent) {
    FPlatformProcess::ReturnSynchEventToPool(StopEvent);
    StopEvent = nullptr;
  }

  if (FSocket *LocalSocket = DetachSocket()) {
    LocalSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LocalSocket);
  }
  CloseNativeSocket();
}

FSocket *FMcpBridgeWebSocket::DetachSocket() {
  return static_cast<FSocket *>(FPlatformAtomics::InterlockedExchangePtr(
      reinterpret_cast<void **>(&Socket), nullptr));
}

void FMcpBridgeWebSocket::CloseListenSocket() {
  FScopeLock Lock(&ListenSocketMutex);
  if (ListenSocket) {
    ListenSocket->Close();
  }
}

void FMcpBridgeWebSocket::DestroyListenSocket() {
  FSocket *SocketToDestroy = nullptr;
  {
    FScopeLock Lock(&ListenSocketMutex);
    SocketToDestroy = ListenSocket;
    ListenSocket = nullptr;
  }

  if (SocketToDestroy) {
    SocketToDestroy->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
        ->DestroySocket(SocketToDestroy);
  }
}

void FMcpBridgeWebSocket::NotifyMessageHandlerRegistered() {
  bHandlerRegistered = true;
  if (HandlerReadyEvent) {
    HandlerReadyEvent->Trigger();
  }
}

void FMcpBridgeWebSocket::InitializeWeakSelf(
    const TSharedPtr<FMcpBridgeWebSocket> &InShared) {
  SelfWeakPtr = InShared;
}

void FMcpBridgeWebSocket::Connect() {
  if (Thread) {
    return;
  }

  bStopping = false;
  StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
  Thread = FRunnableThread::Create(this, TEXT("FMcpBridgeWebSocketWorker"), 0,
                                   TPri_Normal);
  if (!Thread) {
    DispatchOnGameThread([WeakThis = SelfWeakPtr] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(
            TEXT("Failed to create WebSocket worker thread."));
      }
    });
  }
}

void FMcpBridgeWebSocket::Listen() {
  if (Thread || !bServerMode) {
    return;
  }

  bStopping = false;
  StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Spawning MCP automation server thread for %s:%d"), *ListenHost,
         Port);
  Thread = FRunnableThread::Create(
      this, TEXT("FMcpBridgeWebSocketServerWorker"), 0, TPri_Normal);
  if (!Thread) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to create server thread for MCP automation bridge."));
    DispatchOnGameThread([WeakThis = SelfWeakPtr] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(
            TEXT("Failed to create WebSocket server worker thread."));
      }
    });
  }
}

void FMcpBridgeWebSocket::Close(int32 StatusCode, const FString &Reason) {
  if (bCloseStarted.Exchange(true)) {
    return;
  }

  bStopping = true;
  if (StopEvent) {
    StopEvent->Trigger();
  }

  CloseListenSocket();

  TArray<TSharedPtr<FMcpBridgeWebSocket>> SocketsToClose;
  {
    FScopeLock Lock(&ClientSocketsMutex);
    SocketsToClose = MoveTemp(ClientSockets);
  }

  for (TSharedPtr<FMcpBridgeWebSocket> &ClientSock : SocketsToClose) {
    if (ClientSock.IsValid()) {
      ClientSock->Close(StatusCode, Reason);
    }
  }

  if (FSocket *LocalSocket = DetachSocket()) {
    LocalSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LocalSocket);
  }

  ShutdownTls();
  CloseNativeSocket();
}

bool FMcpBridgeWebSocket::Send(const FString &Data) {
  FTCHARToUTF8 Converter(*Data);
  return Send(Converter.Get(), Converter.Length());
}

bool FMcpBridgeWebSocket::Send(const void *Data, SIZE_T Length) {
  if (!IsConnected()) {
    return false;
  }
  if (bUseTls) {
    if (!SslHandle) {
      return false;
    }
  } else if (!Socket) {
    return false;
  }

  return SendTextFrame(Data, Length);
}

bool FMcpBridgeWebSocket::IsConnected() const { return bConnected; }

bool FMcpBridgeWebSocket::IsListening() const { return bListening; }

void FMcpBridgeWebSocket::SendHeartbeatPing() {
  SendControlFrame(OpCodePing, TArray<uint8>());
}

bool FMcpBridgeWebSocket::Init() { return true; }

uint32 FMcpBridgeWebSocket::Run() {
  return bServerMode ? RunServer() : RunClient();
}

void FMcpBridgeWebSocket::Stop() {
  bStopping = true;
  if (StopEvent) {
    StopEvent->Trigger();
  }
}

void FMcpBridgeWebSocket::TearDown(const FString &Reason, bool bWasClean,
                                   int32 StatusCode) {
  if (FSocket *LocalSocket = DetachSocket()) {
    LocalSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(LocalSocket);
  }

  const bool bWasConnected = bConnected;
  bConnected = false;
  ResetFragmentState();

  DispatchOnGameThread([WeakThis = SelfWeakPtr, Reason, bWasClean, StatusCode,
                        bWasConnected] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      if (!bWasConnected) {
        Pinned->ConnectionErrorDelegate.Broadcast(Reason);
      }
      Pinned->ClosedDelegate.Broadcast(Pinned, StatusCode, Reason, bWasClean);
    }
  });
}
