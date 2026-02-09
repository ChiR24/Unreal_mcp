#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeSettings.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "HAL/Event.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Base64.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Misc/StringBuilder.h"
#include "Misc/Timespan.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "String/LexFromString.h"
#include "Ssl.h"
#include "SslModule.h"

#if WITH_SSL

// Work around UI naming conflict: OpenSSL defines UI as a type, but UE defines UI as a namespace
#define UI OSSL_UI

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

// Restore UI after OpenSSL headers
#undef UI

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <unistd.h>
#endif

#endif // WITH_SSL


namespace {
constexpr const TCHAR *WebSocketGuid =
    TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
constexpr uint8 OpCodeContinuation = 0x0;
constexpr uint8 OpCodeText = 0x1;
constexpr uint8 OpCodeBinary = 0x2;
constexpr uint8 OpCodeClose = 0x8;
constexpr uint8 OpCodePing = 0x9;
constexpr uint8 OpCodePong = 0xA;

constexpr uint64 MaxWebSocketMessageBytes = 5ULL * 1024ULL * 1024ULL;
constexpr uint64 MaxWebSocketFramePayloadBytes = MaxWebSocketMessageBytes;
constexpr int32 WebSocketCloseCodeMessageTooBig = 1009;

struct FParsedWebSocketUrl {
  FString Host;
  int32 Port = 80;
  FString PathWithQuery;
  bool bUseTls = false;
};

bool ParseWebSocketUrl(const FString &InUrl, FParsedWebSocketUrl &OutParsed,
                       FString &OutError) {
  const FString Trimmed = InUrl.TrimStartAndEnd();
  if (Trimmed.IsEmpty()) {
    OutError = TEXT("WebSocket URL is empty.");
    return false;
  }

  static const FString SchemePrefix(TEXT("ws://"));
  static const FString SecureSchemePrefix(TEXT("wss://"));
  FString Remainder;
  if (Trimmed.StartsWith(SchemePrefix, ESearchCase::IgnoreCase)) {
    Remainder = Trimmed.Mid(SchemePrefix.Len());
    OutParsed.bUseTls = false;
  } else if (Trimmed.StartsWith(SecureSchemePrefix, ESearchCase::IgnoreCase)) {
    Remainder = Trimmed.Mid(SecureSchemePrefix.Len());
    OutParsed.bUseTls = true;
  } else {
    OutError = TEXT("Only ws:// or wss:// schemes are supported.");
    return false;
  }

  FString HostPort;
  FString PathRemainder;
  if (!Remainder.Split(TEXT("/"), &HostPort, &PathRemainder,
                       ESearchCase::CaseSensitive, ESearchDir::FromStart)) {
    HostPort = Remainder;
    PathRemainder.Reset();
  }

  HostPort = HostPort.TrimStartAndEnd();
  if (HostPort.IsEmpty()) {
    OutError = TEXT("WebSocket URL missing host.");
    return false;
  }

  FString Host;
  int32 Port = 80;

  if (HostPort.StartsWith(TEXT("["))) {
    int32 ClosingBracketIndex = INDEX_NONE;
    if (!HostPort.FindChar(TEXT(']'), ClosingBracketIndex)) {
      OutError = TEXT("Invalid IPv6 WebSocket host.");
      return false;
    }

    Host = HostPort.Mid(1, ClosingBracketIndex - 1);
    const int32 PortSeparatorIndex = HostPort.Find(
        TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (PortSeparatorIndex > ClosingBracketIndex) {
      const FString PortString = HostPort.Mid(PortSeparatorIndex + 1);
      if (!PortString.IsEmpty() && !LexTryParseString(Port, *PortString)) {
        OutError = TEXT("Invalid WebSocket port.");
        return false;
      }
    }
  } else {
    FString PortString;
    if (HostPort.Split(TEXT(":"), &Host, &PortString,
                       ESearchCase::CaseSensitive, ESearchDir::FromEnd)) {
      Host = Host.TrimStartAndEnd();
      PortString = PortString.TrimStartAndEnd();

      if (!PortString.IsEmpty()) {
        if (!LexTryParseString(Port, *PortString)) {
          OutError = TEXT("Invalid WebSocket port.");
          return false;
        }
      }
    } else {
      Host = HostPort;
    }
  }

  Host = Host.TrimStartAndEnd();
  if (Host.IsEmpty()) {
    OutError = TEXT("WebSocket URL missing host.");
    return false;
  }

  if (Port <= 0) {
    OutError = TEXT("WebSocket port must be positive.");
    return false;
  }

  FString PathWithQuery;
  if (PathRemainder.IsEmpty()) {
    PathWithQuery = TEXT("/");
  } else {
    PathWithQuery = TEXT("/") + PathRemainder;
  }

  OutParsed.Host = Host;
  OutParsed.Port = Port;
  OutParsed.PathWithQuery = PathWithQuery;
  return true;
}

uint16 ToNetwork16(uint16 Value) {
#if PLATFORM_LITTLE_ENDIAN
  return static_cast<uint16>(((Value & 0x00FF) << 8) | ((Value & 0xFF00) >> 8));
#else
  return Value;
#endif
}

uint16 FromNetwork16(uint16 Value) { return ToNetwork16(Value); }

uint64 ToNetwork64(uint64 Value) {
#if PLATFORM_LITTLE_ENDIAN
  return ((Value & 0x00000000000000FFULL) << 56) |
         ((Value & 0x000000000000FF00ULL) << 40) |
         ((Value & 0x0000000000FF0000ULL) << 24) |
         ((Value & 0x00000000FF000000ULL) << 8) |
         ((Value & 0x000000FF00000000ULL) >> 8) |
         ((Value & 0x0000FF0000000000ULL) >> 24) |
         ((Value & 0x00FF000000000000ULL) >> 40) |
         ((Value & 0xFF00000000000000ULL) >> 56);
#else
  return Value;
#endif
}

uint64 FromNetwork64(uint64 Value) { return ToNetwork64(Value); }

FString BytesToStringView(const TArray<uint8> &Data) {
  if (Data.Num() == 0) {
    return FString();
  }
  // Convert UTF-8 bytes to TCHAR using an explicit length-aware converter
  // to avoid reading beyond the provided buffer. The previous implementation
  // used a null-terminated style conversion which could read past the
  // payload and include stray bytes from subsequent socket reads, causing
  // JSON parse failures on the receiving side.
  const ANSICHAR *Utf8Ptr = reinterpret_cast<const ANSICHAR *>(Data.GetData());
  FUTF8ToTCHAR Converter(Utf8Ptr, Data.Num());
  if (Converter.Length() <= 0) {
    return FString();
  }
  return FString(Converter.Length(), Converter.Get());
}

void DispatchOnGameThread(TFunction<void()> &&Fn) {
  if (IsInGameThread()) {
    Fn();
    return;
  }

  AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
}

FString DescribeSocketError(ISocketSubsystem *SocketSubsystem,
                            const TCHAR *Context) {
  if (!SocketSubsystem) {
    return FString::Printf(TEXT("%s (no socket subsystem)"), Context);
  }

  const ESocketErrors LastErrorCode = SocketSubsystem->GetLastErrorCode();
  const FString Description = SocketSubsystem->GetSocketError(LastErrorCode);
  return FString::Printf(TEXT("%s (error=%d, %s)"), Context,
                         static_cast<int32>(LastErrorCode), *Description);
}
} // namespace

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
      bUseTls(bInEnableTls), bTlsServer(false), bSslInitialized(false),
      bOwnsSslContext(false), SslContext(nullptr), SslHandle(nullptr), NativeSocketHandle(0),
      bNativeSocketReleased(false), TlsCertificatePath(InTlsCertificatePath),
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
      bListening(false), bStopping(false), bUseTls(bInEnableTls),
      bTlsServer(true), bSslInitialized(false), bOwnsSslContext(false),
      SslContext(nullptr),
      SslHandle(nullptr), NativeSocketHandle(0), bNativeSocketReleased(false),
      TlsCertificatePath(InTlsCertificatePath),
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
      bStopping(false), bUseTls(bInEnableTls), bTlsServer(true),
      bSslInitialized(false), bOwnsSslContext(false), SslContext(nullptr),
      SslHandle(nullptr),
      NativeSocketHandle(0), bNativeSocketReleased(false),
      TlsCertificatePath(InTlsCertificatePath),
      TlsPrivateKeyPath(InTlsPrivateKeyPath) {
  HandlerReadyEvent = nullptr;
  bHandlerRegistered = false;
}

FMcpBridgeWebSocket::~FMcpBridgeWebSocket() {
  Close();
  ShutdownTls();
  if (HandlerReadyEvent) {
    FPlatformProcess::ReturnSynchEventToPool(HandlerReadyEvent);
    HandlerReadyEvent = nullptr;
  }
  
  if (Thread) {
    // Wait for thread completion. The Close() call above should have unblocked
    // any waiting socket operations by destroying ListenSocket/Socket.
    // Use Kill() first to request the thread to stop, then wait for completion.
    Thread->Kill(true); // true = wait for completion
    delete Thread;
    Thread = nullptr;
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

#if WITH_SSL

bool FMcpBridgeWebSocket::InitializeTlsContext(bool bServer) {
  if (!bUseTls) {
    return true;
  }

  if (SslContext) {
    return true;
  }

  FSslModule &SslModule = FSslModule::Get();
  ISslManager &SslManager = SslModule.GetSslManager();
  if (!SslManager.InitializeSsl()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to initialize SSL module."));
    return false;
  }

  bSslInitialized = true;

  if (!bServer) {
    FSslContextCreateOptions Options;
    Options.bAllowCompression = false;
    Options.bAddCertificates = true;
    SslContext = SslManager.CreateSslContext(Options);
    if (!SslContext) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Failed to create SSL client context."));
      return false;
    }
    bOwnsSslContext = false;
    SSL_CTX_set_verify(SslContext, SSL_VERIFY_PEER, nullptr);
    return true;
  }

  const SSL_METHOD *Method = TLS_server_method();
  if (!Method) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to resolve TLS server method."));
    return false;
  }

  SslContext = SSL_CTX_new(Method);
  if (!SslContext) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to create SSL server context."));
    return false;
  }
  bOwnsSslContext = true;

  SSL_CTX_set_min_proto_version(SslContext, TLS1_2_VERSION);
  SSL_CTX_set_options(SslContext, SSL_OP_NO_COMPRESSION);

  if (TlsCertificatePath.IsEmpty() || TlsPrivateKeyPath.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS is enabled but certificate or key path is missing."));
    return false;
  }

  if (!FPaths::FileExists(TlsCertificatePath)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS certificate not found: %s"), *TlsCertificatePath);
    return false;
  }

  if (!FPaths::FileExists(TlsPrivateKeyPath)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS private key not found: %s"), *TlsPrivateKeyPath);
    return false;
  }

  const FTCHARToUTF8 CertPathUtf8(*TlsCertificatePath);
  const FTCHARToUTF8 KeyPathUtf8(*TlsPrivateKeyPath);
  if (SSL_CTX_use_certificate_file(SslContext, CertPathUtf8.Get(), SSL_FILETYPE_PEM) <= 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to load TLS certificate: %s"), *TlsCertificatePath);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(SslContext, KeyPathUtf8.Get(), SSL_FILETYPE_PEM) <= 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to load TLS private key: %s"), *TlsPrivateKeyPath);
    return false;
  }
  if (SSL_CTX_check_private_key(SslContext) != 1) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS private key does not match certificate."));
    return false;
  }

  SSL_CTX_set_verify(SslContext, SSL_VERIFY_NONE, nullptr);
  return true;
}

bool FMcpBridgeWebSocket::EstablishTls(bool bServer) {
  if (!bUseTls) {
    return true;
  }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
  if (!Socket || bNativeSocketReleased) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS requested without a valid socket."));
    return false;
  }

  if (!InitializeTlsContext(bServer)) {
    return false;
  }

  NativeSocketHandle = Socket->ReleaseNativeSocket();
  bNativeSocketReleased = true;
  ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
  Socket = nullptr;

  if (NativeSocketHandle == 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to obtain native socket handle for TLS."));
    return false;
  }

  SslHandle = SSL_new(SslContext);
  if (!SslHandle) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to create SSL connection state."));
    return false;
  }

  SSL_set_fd(SslHandle, static_cast<int>(NativeSocketHandle));
  int Result = bServer ? SSL_accept(SslHandle) : SSL_connect(SslHandle);
  if (Result <= 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS handshake failed (mode=%s)."),
           bServer ? TEXT("server") : TEXT("client"));
    return false;
  }

  bTlsServer = bServer;
  return true;
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
         TEXT("TLS requires UE 5.7 or later. Current version: %d.%d. Cannot establish TLS connection."),
         ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION);
  return false;
#endif
}

void FMcpBridgeWebSocket::ShutdownTls() {
  if (SslHandle) {
    SSL_shutdown(SslHandle);
    SSL_free(SslHandle);
    SslHandle = nullptr;
  }

  if (SslContext && bOwnsSslContext) {
    SSL_CTX_free(SslContext);
    SslContext = nullptr;
  }

  if (bSslInitialized) {
    FSslModule::Get().GetSslManager().ShutdownSsl();
    bSslInitialized = false;
  }
}

void FMcpBridgeWebSocket::CloseNativeSocket() {
  if (NativeSocketHandle == 0) {
    return;
  }
#if PLATFORM_WINDOWS
  closesocket(static_cast<SOCKET>(NativeSocketHandle));
#else
  close(static_cast<int>(NativeSocketHandle));
#endif
  NativeSocketHandle = 0;
}

bool FMcpBridgeWebSocket::SendRaw(const uint8 *Data, int32 Length,
                                 int32 &OutBytesSent) {
  OutBytesSent = 0;
  if (bUseTls && SslHandle) {
    const int Result = SSL_write(SslHandle, Data, Length);
    if (Result > 0) {
      OutBytesSent = Result;
      return true;
    }
    const int ErrorCode = SSL_get_error(SslHandle, Result);
    if (ErrorCode == SSL_ERROR_WANT_READ || ErrorCode == SSL_ERROR_WANT_WRITE) {
      return true;
    }
    return false;
  }

  if (!Socket) {
    return false;
  }

  return Socket->Send(Data, Length, OutBytesSent);
}

bool FMcpBridgeWebSocket::RecvRaw(uint8 *Data, int32 Length,
                                 int32 &OutBytesRead) {
  OutBytesRead = 0;
  if (bUseTls && SslHandle) {
    const int Result = SSL_read(SslHandle, Data, Length);
    if (Result > 0) {
      OutBytesRead = Result;
      return true;
    }
    const int ErrorCode = SSL_get_error(SslHandle, Result);
    if (ErrorCode == SSL_ERROR_WANT_READ || ErrorCode == SSL_ERROR_WANT_WRITE) {
      return true;
    }
    return false;
  }

  if (!Socket) {
    return false;
  }

  return Socket->Recv(Data, Length, OutBytesRead);
}

#else // !WITH_SSL

bool FMcpBridgeWebSocket::InitializeTlsContext(bool bServer) {
  if (bUseTls) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("TLS requested but WITH_SSL is not enabled."));
  }
  return !bUseTls;
}

bool FMcpBridgeWebSocket::EstablishTls(bool bServer) {
  if (bUseTls) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("TLS requested but WITH_SSL is not enabled."));
    return false;
  }
  return true;
}

void FMcpBridgeWebSocket::ShutdownTls() {
  // No-op when TLS is not available
}

void FMcpBridgeWebSocket::CloseNativeSocket() {
  // Native socket handle is only used when TLS/SSL is enabled.
  // When WITH_SSL is disabled, this is a no-op.
#if WITH_SSL
  if (NativeSocketHandle == 0) {
    return;
  }
#if PLATFORM_WINDOWS
  closesocket(static_cast<SOCKET>(NativeSocketHandle));
#else
  close(static_cast<int>(NativeSocketHandle));
#endif
  NativeSocketHandle = 0;
#endif // WITH_SSL
}

bool FMcpBridgeWebSocket::SendRaw(const uint8 *Data, int32 Length,
                                 int32 &OutBytesSent) {
  OutBytesSent = 0;
  if (!Socket) {
    return false;
  }
  return Socket->Send(Data, Length, OutBytesSent);
}

bool FMcpBridgeWebSocket::RecvRaw(uint8 *Data, int32 Length,
                                 int32 &OutBytesRead) {
  OutBytesRead = 0;
  if (!Socket) {
    return false;
  }
  return Socket->Recv(Data, Length, OutBytesRead);
}

#endif // WITH_SSL

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
  bStopping = true;
  if (StopEvent) {
    StopEvent->Trigger();
  }

  // Close the listen socket to unblock Accept() in RunServer().
  // IMPORTANT: We only close here, NOT destroy. RunServer() owns the socket and
  // will destroy it after its loop exits. This avoids a TOCTOU race where we
  // destroy the socket while RunServer() is between checking ListenSocket and
  // calling Accept().
  if (ListenSocket) {
    ListenSocket->Close();
  }

  // Close any client sockets that were accepted by this server
  TArray<TSharedPtr<FMcpBridgeWebSocket>> SocketsToClose;
  {
    FScopeLock Lock(&ClientSocketsMutex);
    SocketsToClose = MoveTemp(ClientSockets);
  }

  for (TSharedPtr<FMcpBridgeWebSocket>& ClientSock : SocketsToClose) {
    if (ClientSock.IsValid()) {
      ClientSock->Close(StatusCode, Reason);
    }
  }

  // Close the main socket (for client connections)
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
  if (bServerMode) {
    return RunServer();
  } else {
    return RunClient();
  }
}

uint32 FMcpBridgeWebSocket::RunClient() {
  if (bServerAcceptedConnection) {
    if (!PerformServerHandshake()) {
      return 0;
    }
  } else {
    if (!PerformHandshake()) {
      return 0;
    }
  }

  bConnected = true;
  UE_LOG(
      LogMcpAutomationBridgeSubsystem, Log,
      TEXT("FMcpBridgeWebSocket connection established (serverAccepted=%s)."),
      bServerAcceptedConnection ? TEXT("true") : TEXT("false"));
  DispatchOnGameThread([WeakThis = SelfWeakPtr] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      Pinned->ConnectedDelegate.Broadcast(Pinned);
    }
  });

  // If this connection was accepted by the server thread (i.e. a remote
  // client connected to the plugin), wait a short time for the game
  // thread to attach message handlers. The client is likely to send the
  // application-level 'bridge_hello' immediately after the upgrade; if
  // the game thread hasn't attached its OnMessage handler yet we risk
  // losing that first frame. Wait up to a moderate timeout for the
  // handler registration signal.
  if (bServerAcceptedConnection) {
    // Lazily create the event used to wait for the handler if it
    // hasn't been allocated yet. Use the event pool to avoid
    // continuously allocating objects.
    if (!HandlerReadyEvent) {
      HandlerReadyEvent = FPlatformProcess::GetSynchEventFromPool(true);
    }

    constexpr double MaxWaitSeconds = 0.5; // 500 ms
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Awaiting message handler registration for new client "
                "connection (max %.0f ms)."),
           MaxWaitSeconds * 1000.0);
    if (HandlerReadyEvent->Wait(FTimespan::FromSeconds(MaxWaitSeconds))) {
      // Event triggered by game thread
    }
    if (!bHandlerRegistered) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Message handler registration not observed in time; "
                  "proceeding without explicit synchronization."));
    }
  }

  while (!bStopping) {
    if (!ReceiveFrame()) {
      break;
    }
  }

  TearDown(TEXT("Socket loop finished."), true, 1000);
  return 0;
}

uint32 FMcpBridgeWebSocket::RunServer() {
  // Determine if we need IPv6 socket based on host address
  const bool bIsIpv6Host = ListenHost.Contains(TEXT(":"));
  
  ISocketSubsystem *SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("FMcpBridgeWebSocket::RunServer begin (host=%s, port=%d, IPv6=%s)"),
         *ListenHost, Port, bIsIpv6Host ? TEXT("true") : TEXT("false"));
  
  // Create socket with proper protocol family for IPv6 support
  // Use FName-based protocol specification (non-deprecated API)
  const FName ProtocolName = bIsIpv6Host ? FName(TEXT("IPv6")) : FName();
  ListenSocket = SocketSubsystem->CreateSocket(
      NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), ProtocolName);
  if (!ListenSocket) {
    const FString ErrorMessage = DescribeSocketError(
        SocketSubsystem, TEXT("Failed to create listen socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }

  ListenSocket->SetReuseAddr(true);
  ListenSocket->SetNonBlocking(false);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Listen socket created."));

  TSharedRef<FInternetAddr> ListenAddr = SocketSubsystem->CreateInternetAddr();

  bool bResolvedHost = false;

  FString HostToBind = ListenHost.TrimStartAndEnd();
  if (HostToBind.IsEmpty()) {
    HostToBind = TEXT("127.0.0.1");
  }
  if (HostToBind.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)) {
    HostToBind = TEXT("127.0.0.1");
  }

  const bool bIsLoopback = HostToBind.Equals(TEXT("127.0.0.1"), ESearchCase::IgnoreCase) ||
                           HostToBind.Equals(TEXT("::1"), ESearchCase::IgnoreCase);

  // Check if non-loopback binding is allowed via settings
  const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
  const bool bAllowNonLoopback = Settings ? Settings->bAllowNonLoopback : false;

  if (bIsLoopback) {
    bool bIsValidIp = false;
    ListenAddr->SetIp(*HostToBind, bIsValidIp);
    bResolvedHost = bIsValidIp;

    // Fallback to 127.0.0.1 if IPv6 loopback (::1) fails on systems without IPv6 support
    if (!bResolvedHost && HostToBind.Equals(TEXT("::1"), ESearchCase::IgnoreCase)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("IPv6 loopback '::1' not supported on this system. Falling back to 127.0.0.1."));
      
      // Re-create socket as IPv4 since we're falling back to IPv4 address
      SocketSubsystem->DestroySocket(ListenSocket);
      ListenSocket = SocketSubsystem->CreateSocket(
          NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), FName());
      if (!ListenSocket) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Failed to re-create IPv4 socket for fallback."));
        return 0;
      }
      ListenSocket->SetReuseAddr(true);
      ListenSocket->SetNonBlocking(false);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Re-created socket for IPv4 fallback."));
      
      bool bFallbackIsValidIp = false;
      ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
      bResolvedHost = bFallbackIsValidIp;
    }
  }
  else if (bAllowNonLoopback) {
    // LAN binding enabled - allow non-loopback addresses with security warning
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SECURITY: Binding to non-loopback address '%s'. The automation bridge is exposed to your local network."),
           *HostToBind);

    bool bIsValidIp = false;
    ListenAddr->SetIp(*HostToBind, bIsValidIp);
    bResolvedHost = bIsValidIp;

    // If not a valid IP, try to resolve as hostname via DNS
    if (!bResolvedHost) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("'%s' is not a valid IP address. Attempting DNS resolution..."),
             *HostToBind);
      
      FAddressInfoResult AddrInfoResult = SocketSubsystem->GetAddressInfo(
          *HostToBind, 
          nullptr, 
          EAddressInfoFlags::Default,
          NAME_None,
          ESocketType::SOCKTYPE_Streaming);
      
      if (AddrInfoResult.ReturnCode == SE_NO_ERROR && AddrInfoResult.Results.Num() > 0) {
        // Use the first resolved address
        ListenAddr = AddrInfoResult.Results[0].Address;
        bResolvedHost = true;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Successfully resolved '%s' to address '%s'."),
               *HostToBind, *ListenAddr->ToString(true));
        
        // Check if resolved address family matches socket family
        // UE 5.7 uses GetProtocolType() which returns FName instead of GetProtocolFamily()
        const FName ProtocolType = ListenAddr->GetProtocolType();
        const bool bResolvedIsIpv6 = (ProtocolType == FName(TEXT("IPv6")));
        if (bResolvedIsIpv6 != bIsIpv6Host) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("DNS resolved to %s but socket is %s. Recreating socket..."),
                 bResolvedIsIpv6 ? TEXT("IPv6") : TEXT("IPv4"),
                 bIsIpv6Host ? TEXT("IPv6") : TEXT("IPv4"));
          
          SocketSubsystem->DestroySocket(ListenSocket);
          const FName NewProtocolName = bResolvedIsIpv6 ? FName(TEXT("IPv6")) : FName();
          ListenSocket = SocketSubsystem->CreateSocket(
              NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), NewProtocolName);
          if (!ListenSocket) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, 
                   TEXT("Failed to re-create socket for resolved address family."));
            return 0;
          }
          ListenSocket->SetReuseAddr(true);
          ListenSocket->SetNonBlocking(false);
        }
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to resolve hostname '%s'. Falling back to 127.0.0.1."),
               *HostToBind);
        bool bFallbackIsValidIp = false;
        ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
        bResolvedHost = bFallbackIsValidIp;
      }
    }
  }
  else {
    // Loopback-only mode (default) - reject non-loopback addresses
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("ListenHost '%s' is not a loopback address and bAllowNonLoopback is false. Falling back to 127.0.0.1. Enable 'Allow Non Loop Back' in Project Settings to use LAN addresses."),
           *HostToBind);

    // If socket was created as IPv6 but we're falling back to IPv4, recreate it
    if (bIsIpv6Host) {
      SocketSubsystem->DestroySocket(ListenSocket);
      ListenSocket = SocketSubsystem->CreateSocket(
          NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), FName());
      if (!ListenSocket) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Failed to re-create IPv4 socket for fallback."));
        return 0;
      }
      ListenSocket->SetReuseAddr(true);
      ListenSocket->SetNonBlocking(false);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Re-created socket for IPv4 fallback (non-loopback path)."));
    }

    bool bFallbackIsValidIp = false;
    ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
    bResolvedHost = bFallbackIsValidIp;
  }

  ListenAddr->SetPort(Port);

  if (!ListenSocket->Bind(*ListenAddr)) {
    const FString ErrorMessage = DescribeSocketError(
        SocketSubsystem, TEXT("Failed to bind listen socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Listen socket bound to %s."), *ListenAddr->ToString(false));

  if (!ListenSocket->Listen(ListenBacklog > 0 ? ListenBacklog : 10)) {
    const FString ErrorMessage = DescribeSocketError(
        SocketSubsystem, TEXT("Failed to listen on socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }

  bListening = true;
  const FString BoundAddress = ListenAddr->ToString(false);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("MCP Automation Bridge listening on %s"), *BoundAddress);
  DispatchOnGameThread([WeakThis = SelfWeakPtr, BoundAddress] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      Pinned->ConnectedDelegate.Broadcast(Pinned); // Server ready event
    }
  });

  while (!bStopping && ListenSocket) {
    // Note: Accept() blocks until a connection arrives or the socket is closed.
    // Close() calls ListenSocket->Close() to unblock this call during shutdown.
    // This thread owns ListenSocket destruction (done after loop exits).
    FSocket *ClientSocket =
        ListenSocket->Accept(TEXT("McpAutomationBridgeClient"));
    
    // Check again after Accept() returns - socket may have been closed
    if (bStopping || !ListenSocket) {
      if (ClientSocket) {
        // Clean up any socket we accepted during shutdown race
        ClientSocket->Close();
        SocketSubsystem->DestroySocket(ClientSocket);
      }
      break;
    }
    
    if (ClientSocket) {
      TSharedRef<FInternetAddr> PeerAddr = SocketSubsystem->CreateInternetAddr();
      if (ClientSocket->GetPeerAddress(*PeerAddr)) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Accepted automation client from %s"),
               *PeerAddr->ToString(true));
      }
      // Create a new WebSocket instance for this client connection
      auto ClientWebSocket = MakeShared<FMcpBridgeWebSocket>(
          ClientSocket, bUseTls, TlsCertificatePath, TlsPrivateKeyPath);
      ClientWebSocket->InitializeWeakSelf(ClientWebSocket);
      ClientWebSocket->bServerMode =
          false; // Client connections are not in server mode
      ClientWebSocket->bServerAcceptedConnection =
          true; // This is a server-accepted connection
      // Annotate the accepted client socket with the server listening port
      // so diagnostic logs and handshake acknowledgements report a
      // meaningful activePort instead of 0.
      ClientWebSocket->Port = Port;

      {
        FScopeLock Lock(&ClientSocketsMutex);
        ClientSockets.Add(ClientWebSocket);
      }

      TWeakPtr<FMcpBridgeWebSocket> LocalWeakThis = SelfWeakPtr;
      auto RemoveFromClientList = [LocalWeakThis, ClientWebSocket] {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin()) {
          FScopeLock Lock(&Pinned->ClientSocketsMutex);
          UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
                 TEXT("Removing client socket from server tracking (remaining "
                      "before remove: %d)."),
                 Pinned->ClientSockets.Num());
          Pinned->ClientSockets.Remove(ClientWebSocket);
        }
      };

      ClientWebSocket->OnConnected().AddLambda(
          [LocalWeakThis, ClientWebSocket](TSharedPtr<FMcpBridgeWebSocket>) {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin()) {
              DispatchOnGameThread(
                  [ParentWeak = LocalWeakThis, ClientSocket = ClientWebSocket] {
                    if (TSharedPtr<FMcpBridgeWebSocket> DispatchPinned =
                            ParentWeak.Pin()) {
                      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                             TEXT("Broadcasting client connected delegate."));
                      DispatchPinned->ClientConnectedDelegate.Broadcast(
                          ClientSocket);
                    }
                  });
            }
          });

      ClientWebSocket->OnClosed().AddLambda(
          [RemoveFromClientList](TSharedPtr<FMcpBridgeWebSocket>, int32,
                                 const FString &,
                                 bool) { RemoveFromClientList(); });

      ClientWebSocket->OnConnectionError().AddLambda(
          [RemoveFromClientList](const FString &) { RemoveFromClientList(); });

      // Start the client WebSocket thread to handle the handshake and
      // communication
      ClientWebSocket->Connect();
    } else {
      // Sleep briefly to avoid busy waiting
      FPlatformProcess::Sleep(AcceptSleepSeconds > 0.0f ? AcceptSleepSeconds
                                                        : 0.01f);
    }
  }

  if (ListenSocket) {
    ListenSocket->Close();
    SocketSubsystem->DestroySocket(ListenSocket);
    ListenSocket = nullptr;
  }

  return 0;
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

bool FMcpBridgeWebSocket::PerformHandshake() {
  FParsedWebSocketUrl ParsedUrl;
  FString ParseError;
  if (!ParseWebSocketUrl(Url, ParsedUrl, ParseError)) {
    TearDown(ParseError, false, 4000);
    return false;
  }

  if (bUseTls && !ParsedUrl.bUseTls) {
    TearDown(TEXT("TLS is enabled but ws:// URL was provided."), false, 4000);
    return false;
  }
  if (ParsedUrl.bUseTls) {
    bUseTls = true;
  }

  HostHeader = ParsedUrl.Host;
  Port = ParsedUrl.Port;
  HandshakePath = ParsedUrl.PathWithQuery;

  TSharedPtr<FInternetAddr> Endpoint;
  if (!ResolveEndpoint(Endpoint) || !Endpoint.IsValid()) {
    TearDown(TEXT("Unable to resolve WebSocket host."), false, 4000);
    return false;
  }

  ISocketSubsystem *SocketSubsystem =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  Socket = SocketSubsystem->CreateSocket(
      NAME_Stream, TEXT("McpAutomationBridgeSocket"), false);
  if (!Socket) {
    TearDown(TEXT("Failed to create socket."), false, 4000);
    return false;
  }
  Socket->SetReuseAddr(true);
  Socket->SetNonBlocking(false);
  Socket->SetNoDelay(true);

  Endpoint->SetPort(Port);
  if (!Socket->Connect(*Endpoint)) {
    TearDown(TEXT("Unable to connect to WebSocket endpoint."), false, 4000);
    return false;
  }

  if (bUseTls) {
    if (!EstablishTls(false)) {
      TearDown(TEXT("TLS handshake failed."), false, 4000);
      return false;
    }
  }

  TArray<uint8> KeyBytes;
  KeyBytes.SetNumUninitialized(16);
  for (uint8 &Byte : KeyBytes) {
    Byte = static_cast<uint8>(FMath::RandRange(0, 255));
  }
  HandshakeKey = FBase64::Encode(KeyBytes.GetData(), KeyBytes.Num());

  FString HostLine = HostHeader;
  const bool bIsIpv6Host = HostLine.Contains(TEXT(":"));
  if (bIsIpv6Host && !HostLine.StartsWith(TEXT("["))) {
    HostLine = FString::Printf(TEXT("[%s]"), *HostLine);
  }
  if (!(Port == 80 || Port == 0)) {
    HostLine += FString::Printf(TEXT(":%d"), Port);
  }

  TStringBuilder<512> RequestBuilder;
  RequestBuilder << TEXT("GET ") << HandshakePath << TEXT(" HTTP/1.1\r\n");
  RequestBuilder << TEXT("Host: ") << HostLine << TEXT("\r\n");
  RequestBuilder << TEXT("Upgrade: websocket\r\n");
  RequestBuilder << TEXT("Connection: Upgrade\r\n");
  RequestBuilder << TEXT("Sec-WebSocket-Version: 13\r\n");
  RequestBuilder << TEXT("Sec-WebSocket-Key: ") << HandshakeKey << TEXT("\r\n");

  if (!Protocols.IsEmpty()) {
    RequestBuilder << TEXT("Sec-WebSocket-Protocol: ") << Protocols
                   << TEXT("\r\n");
  }

  for (const TPair<FString, FString> &HeaderPair : Headers) {
    RequestBuilder << HeaderPair.Key << TEXT(": ") << HeaderPair.Value
                   << TEXT("\r\n");
  }

  RequestBuilder << TEXT("\r\n");

  FTCHARToUTF8 HandshakeUtf8(RequestBuilder.ToString());
  int32 BytesSent = 0;
  if (!SendRaw(reinterpret_cast<const uint8 *>(HandshakeUtf8.Get()),
               HandshakeUtf8.Length(), BytesSent) ||
      BytesSent != HandshakeUtf8.Length()) {
    TearDown(TEXT("Failed to send WebSocket handshake."), false, 4000);
    return false;
  }

  TArray<uint8> ResponseBuffer;
  ResponseBuffer.Reserve(512);
  constexpr int32 TempSize = 256;
  uint8 Temp[TempSize];
  bool bHandshakeComplete = false;
  while (!bHandshakeComplete) {
    if (bStopping) {
      return false;
    }
    int32 BytesRead = 0;
    if (!RecvRaw(Temp, TempSize, BytesRead)) {
      TearDown(TEXT("WebSocket handshake failed while reading response."),
               false, 4000);
      return false;
    }
    if (BytesRead <= 0) {
      continue;
    }
    ResponseBuffer.Append(Temp, BytesRead);
    if (ResponseBuffer.Num() >= 4) {
      const int32 Count = ResponseBuffer.Num();
      if (ResponseBuffer[Count - 4] == '\r' &&
          ResponseBuffer[Count - 3] == '\n' &&
          ResponseBuffer[Count - 2] == '\r' &&
          ResponseBuffer[Count - 1] == '\n') {
        bHandshakeComplete = true;
      }
    }
  }

  FString ResponseString = FString(
      ANSI_TO_TCHAR(reinterpret_cast<const char *>(ResponseBuffer.GetData())));
  FString HeaderSection;
  FString ExtraData;
  if (!ResponseString.Split(TEXT("\r\n\r\n"), &HeaderSection, &ExtraData)) {
    HeaderSection = ResponseString;
  }

  TArray<FString> HeaderLines;
  HeaderSection.ParseIntoArrayLines(HeaderLines, false);
  if (HeaderLines.Num() == 0) {
    TearDown(TEXT("Malformed WebSocket handshake response."), false, 4000);
    return false;
  }

  const FString &StatusLine = HeaderLines[0];
  if (!StatusLine.Contains(TEXT("101"))) {
    TearDown(TEXT("WebSocket server rejected handshake."), false, 4000);
    return false;
  }

  FString ExpectedAccept;
  {
    FTCHARToUTF8 AcceptUtf8(*(HandshakeKey + WebSocketGuid));
    FSHA1 Hash;
    Hash.Update(reinterpret_cast<const uint8 *>(AcceptUtf8.Get()),
                AcceptUtf8.Length());
    Hash.Final();
    uint8 Digest[FSHA1::DigestSize];
    Hash.GetHash(Digest);
    ExpectedAccept = FBase64::Encode(Digest, FSHA1::DigestSize);
  }

  bool bAcceptValid = false;
  for (int32 i = 1; i < HeaderLines.Num(); ++i) {
    FString Key;
    FString Value;
    if (HeaderLines[i].Split(TEXT(":"), &Key, &Value)) {
      Key = Key.TrimStartAndEnd();
      Value = Value.TrimStartAndEnd();
      if (Key.Equals(TEXT("Sec-WebSocket-Accept"), ESearchCase::IgnoreCase)) {
        bAcceptValid = Value.Equals(ExpectedAccept, ESearchCase::CaseSensitive);
      }
    }
  }

  if (!bAcceptValid) {
    TearDown(TEXT("WebSocket handshake validation failed."), false, 4000);
    return false;
  }

  if (!ExtraData.IsEmpty()) {
    const FTCHARToUTF8 ExtraUtf8(*ExtraData);
    PendingReceived.Append(reinterpret_cast<const uint8 *>(ExtraUtf8.Get()),
                           ExtraUtf8.Length());
  }

  return true;
}

bool FMcpBridgeWebSocket::PerformServerHandshake() {
  // Read the client's WebSocket upgrade request
  TArray<uint8> RequestBuffer;
  RequestBuffer.Reserve(1024);
  constexpr int32 TempSize = 256;
  uint8 Temp[TempSize];
  bool bRequestComplete = false;
  FString ClientKey;

  int32 HeaderEndIndex = -1;
  if (bUseTls) {
    if (!EstablishTls(true)) {
      TearDown(TEXT("TLS handshake failed."), false, 4000);
      return false;
    }
  }

  while (!bRequestComplete) {
    if (bStopping) {
      return false;
    }

    int32 BytesRead = 0;
    if (!RecvRaw(Temp, TempSize, BytesRead)) {
      // This may occur when a client connects but immediately closes
      // or when a non-WebSocket probe connects; log at Verbose to avoid
      // spamming warnings for transient or benign network activity.
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Server handshake recv failed while awaiting upgrade request "
                  "(benign or client closed)."));
      TearDown(TEXT("Failed to read WebSocket upgrade request."), false, 4000);
      return false;
    }

    if (BytesRead <= 0) {
      continue;
    }

    RequestBuffer.Append(Temp, BytesRead);

    // Check if we have a complete HTTP request (double CRLF) anywhere
    // in the buffer. Clients may send additional bytes immediately after
    // the headers (for example, the first WebSocket frame), so search
    // the whole buffer and capture any trailing bytes beyond the header
    // terminator into PendingReceived for the frame parser.
    if (RequestBuffer.Num() >= 4) {
      for (int32 Idx = 0; Idx + 3 < RequestBuffer.Num(); ++Idx) {
        if (RequestBuffer[Idx] == '\r' && RequestBuffer[Idx + 1] == '\n' &&
            RequestBuffer[Idx + 2] == '\r' && RequestBuffer[Idx + 3] == '\n') {
          HeaderEndIndex = Idx + 4;
          bRequestComplete = true;
          break;
        }
      }
    }
  }

  FString RequestString = FString(
      ANSI_TO_TCHAR(reinterpret_cast<const char *>(RequestBuffer.GetData())));
  TArray<FString> RequestLines;
  RequestString.ParseIntoArrayLines(RequestLines, false);

  if (RequestLines.Num() == 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake received empty upgrade request."));
    TearDown(TEXT("Malformed WebSocket upgrade request."), false, 4000);
    return false;
  }

  // If there were any bytes received after the HTTP header terminator,
  // preserve them so the frame parser can consume an arriving WebSocket
  // frame that arrived in the same TCP packet as the upgrade request.
  if (HeaderEndIndex > 0 && HeaderEndIndex < RequestBuffer.Num()) {
    const int32 ExtraCount = RequestBuffer.Num() - HeaderEndIndex;
    if (ExtraCount > 0) {
      FScopeLock Guard(&ReceiveMutex);
      PendingReceived.Append(RequestBuffer.GetData() + HeaderEndIndex,
                             ExtraCount);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Server handshake: preserved %d extra bytes after upgrade "
                  "request for subsequent frame parsing."),
             ExtraCount);
    }
  }

  // Parse the request
  bool bValidUpgrade = false;
  bool bValidConnection = false;
  bool bValidVersion = false;
  FString RequestedProtocols;

  for (int32 i = 1; i < RequestLines.Num(); ++i) {
    FString Key, Value;
    if (RequestLines[i].Split(TEXT(":"), &Key, &Value)) {
      Key = Key.TrimStartAndEnd();
      Value = Value.TrimStartAndEnd();

      if (Key.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase) &&
          Value.Equals(TEXT("websocket"), ESearchCase::IgnoreCase)) {
        bValidUpgrade = true;
      } else if (Key.Equals(TEXT("Connection"), ESearchCase::IgnoreCase) &&
                 Value.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase)) {
        bValidConnection = true;
      } else if (Key.Equals(TEXT("Sec-WebSocket-Version"),
                            ESearchCase::IgnoreCase) &&
                 Value.Equals(TEXT("13"), ESearchCase::CaseSensitive)) {
        bValidVersion = true;
      } else if (Key.Equals(TEXT("Sec-WebSocket-Key"),
                            ESearchCase::IgnoreCase)) {
        ClientKey = Value;
      } else if (Key.Equals(TEXT("Sec-WebSocket-Protocol"),
                            ESearchCase::IgnoreCase)) {
        RequestedProtocols = Value;
      }
    }
  }

  if (!bValidUpgrade || !bValidConnection || !bValidVersion ||
      ClientKey.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake validation failed (upgrade=%s, "
                "connection=%s, version=%s, hasKey=%s)."),
           bValidUpgrade ? TEXT("true") : TEXT("false"),
           bValidConnection ? TEXT("true") : TEXT("false"),
           bValidVersion ? TEXT("true") : TEXT("false"),
           ClientKey.IsEmpty() ? TEXT("false") : TEXT("true"));
    TearDown(TEXT("Invalid WebSocket upgrade request."), false, 4000);
    return false;
  }

  // Generate the accept key
  const FString AcceptGuid = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  FString AcceptKey;
  {
    FTCHARToUTF8 AcceptUtf8(*(ClientKey + AcceptGuid));
    FSHA1 Hash;
    Hash.Update(reinterpret_cast<const uint8 *>(AcceptUtf8.Get()),
                AcceptUtf8.Length());
    Hash.Final();
    uint8 Digest[FSHA1::DigestSize];
    Hash.GetHash(Digest);
    AcceptKey = FBase64::Encode(Digest, FSHA1::DigestSize);
  }

  FString SelectedProtocol;
  if (!Protocols.IsEmpty() && !RequestedProtocols.IsEmpty()) {
    TArray<FString> RequestedList;
    RequestedProtocols.ParseIntoArray(RequestedList, TEXT(","), true);

    TArray<FString> SupportedList;
    Protocols.ParseIntoArray(SupportedList, TEXT(","), true);

    for (const FString &Requested : RequestedList) {
      const FString TrimmedRequested = Requested.TrimStartAndEnd();
      for (const FString &Supported : SupportedList) {
        if (TrimmedRequested.Equals(Supported.TrimStartAndEnd(),
                                    ESearchCase::IgnoreCase)) {
          SelectedProtocol = Supported.TrimStartAndEnd();
          break;
        }
      }
      if (!SelectedProtocol.IsEmpty()) {
        break;
      }
    }
  }

  if (!RequestedProtocols.IsEmpty() && SelectedProtocol.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake failed: no matching subprotocol. "
                "Requested=%s Supported=%s"),
           *RequestedProtocols, *Protocols);
    TearDown(TEXT("No matching WebSocket subprotocol."), false, 4403);
    return false;
  }

  // Send the upgrade response
  FString Response = FString::Printf(TEXT("HTTP/1.1 101 Switching Protocols\r\n"
                                          "Upgrade: websocket\r\n"
                                          "Connection: Upgrade\r\n"
                                          "Sec-WebSocket-Accept: %s\r\n"),
                                     *AcceptKey);

  if (!SelectedProtocol.IsEmpty()) {
    Response += FString::Printf(TEXT("Sec-WebSocket-Protocol: %s\r\n"),
                                *SelectedProtocol);
  }

  Response += TEXT("\r\n");

  FTCHARToUTF8 ResponseUtf8(*Response);
  int32 BytesSent = 0;
  if (!SendRaw(reinterpret_cast<const uint8 *>(ResponseUtf8.Get()),
               ResponseUtf8.Length(), BytesSent) ||
      BytesSent != ResponseUtf8.Length()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake failed: unable to send upgrade response "
                "(sent %d expected %d)."),
           BytesSent, ResponseUtf8.Length());
    TearDown(TEXT("Failed to send WebSocket upgrade response."), false, 4000);
    return false;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Server handshake completed; subprotocol=%s"),
         SelectedProtocol.IsEmpty() ? TEXT("(none)") : *SelectedProtocol);

  return true;
}

bool FMcpBridgeWebSocket::ResolveEndpoint(TSharedPtr<FInternetAddr> &OutAddr) {
  ISocketSubsystem *SocketSubsystem =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  if (!SocketSubsystem) {
    return false;
  }

  const FString ServiceName = FString::FromInt(Port);
  FAddressInfoResult AddrInfo = SocketSubsystem->GetAddressInfo(
      *HostHeader, *ServiceName, EAddressInfoFlags::Default, NAME_None,
      ESocketType::SOCKTYPE_Streaming);
  if (AddrInfo.Results.Num() == 0) {
    return false;
  }

  OutAddr = AddrInfo.Results[0].Address;
  if (OutAddr.IsValid()) {
    OutAddr->SetPort(Port);
  }
  return OutAddr.IsValid();
}

bool FMcpBridgeWebSocket::SendFrame(const TArray<uint8> &Frame) {
  if (!Socket && !(bUseTls && SslHandle)) {
    return false;
  }

  int32 TotalBytesSent = 0;
  const int32 TotalBytesToSend = Frame.Num();

  while (TotalBytesSent < TotalBytesToSend) {
    int32 BytesSent = 0;
    if (!SendRaw(Frame.GetData() + TotalBytesSent,
                 TotalBytesToSend - TotalBytesSent, BytesSent)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Socket Send failed after sending %d / %d bytes"),
             TotalBytesSent, TotalBytesToSend);
      return false;
    }

    if (BytesSent <= 0) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Socket Send returned %d bytes (expected > 0). Closing "
                  "connection."),
             BytesSent);
      return false;
    }

    TotalBytesSent += BytesSent;
  }

  return true;
}

bool FMcpBridgeWebSocket::SendCloseFrame(int32 StatusCode,
                                         const FString &Reason) {
  TArray<uint8> Payload;
  Payload.Reserve(2 + Reason.Len() * 4);

  const uint16 Code = ToNetwork16(static_cast<uint16>(StatusCode));
  Payload.Append(reinterpret_cast<const uint8 *>(&Code), sizeof(uint16));

  FTCHARToUTF8 ReasonUtf8(*Reason);
  const int32 ReasonBytes = FMath::Min<int32>(
      ReasonUtf8.Length(),
      123); // ensure control frame payload stays within 125 bytes
  if (ReasonBytes > 0) {
    Payload.Append(reinterpret_cast<const uint8 *>(ReasonUtf8.Get()),
                   ReasonBytes);
  }

  return SendControlFrame(OpCodeClose, Payload);
}

bool FMcpBridgeWebSocket::SendTextFrame(const void *Data, SIZE_T Length) {
  const uint8 *Raw = static_cast<const uint8 *>(Data);
  TArray<uint8> Frame;

  const uint8 Header = 0x80 | OpCodeText;
  Frame.Add(Header);

  const bool bMask = !bServerAcceptedConnection;

  if (Length <= 125) {
    Frame.Add((bMask ? 0x80 : 0x00) | static_cast<uint8>(Length));
  } else if (Length <= 0xFFFF) {
    Frame.Add((bMask ? 0x80 : 0x00) | 126);
    const uint16 SizeShort = ToNetwork16(static_cast<uint16>(Length));
    Frame.Append(reinterpret_cast<const uint8 *>(&SizeShort), sizeof(uint16));
  } else {
    Frame.Add((bMask ? 0x80 : 0x00) | 127);
    const uint64 SizeLong = ToNetwork64(static_cast<uint64>(Length));
    Frame.Append(reinterpret_cast<const uint8 *>(&SizeLong), sizeof(uint64));
  }

  if (bMask) {
    uint8 MaskKey[4];
    for (uint8 &Byte : MaskKey) {
      Byte = static_cast<uint8>(FMath::RandRange(0, 255));
    }
    Frame.Append(MaskKey, 4);

    const int64 Offset = Frame.Num();
    Frame.AddUninitialized(Length);
    for (SIZE_T Index = 0; Index < Length; ++Index) {
      Frame[Offset + Index] = Raw[Index] ^ MaskKey[Index % 4];
    }
  } else {
    Frame.Append(Raw, static_cast<int32>(Length));
  }

  FScopeLock Guard(&SendMutex);
  return SendFrame(Frame);
}

bool FMcpBridgeWebSocket::SendControlFrame(const uint8 ControlOpCode,
                                           const TArray<uint8> &Payload) {
  if (!Socket && !(bUseTls && SslHandle)) {
    return false;
  }

  if (Payload.Num() > 125) {
    return false;
  }

  FScopeLock Guard(&SendMutex);

  TArray<uint8> Frame;
  Frame.Reserve(2 + 4 + Payload.Num());
  Frame.Add(0x80 | (ControlOpCode & 0x0F));
  const bool bMask = !bServerAcceptedConnection;
  Frame.Add((bMask ? 0x80 : 0x00) | static_cast<uint8>(Payload.Num()));

  if (bMask) {
    uint8 MaskKey[4];
    for (uint8 &Byte : MaskKey) {
      Byte = static_cast<uint8>(FMath::RandRange(0, 255));
    }

    Frame.Append(MaskKey, 4);
    const int32 PayloadOffset = Frame.Num();
    Frame.AddUninitialized(Payload.Num());
    for (int32 Index = 0; Index < Payload.Num(); ++Index) {
      Frame[PayloadOffset + Index] = Payload[Index] ^ MaskKey[Index % 4];
    }
  } else if (Payload.Num() > 0) {
    Frame.Append(Payload);
  }

  return SendFrame(Frame);
}

void FMcpBridgeWebSocket::HandleTextPayload(const TArray<uint8> &Payload) {
  const FString Message = BytesToStringView(Payload);
  // Dispatch message handling to the game thread.
  // Many automation handlers touch editor/world state and must run on the
  // game thread. Keeping the socket receive loop thread-free also prevents
  // long-running actions (e.g. export_level) from stalling the connection.
  DispatchOnGameThread([WeakThis = SelfWeakPtr, Message] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      Pinned->MessageDelegate.Broadcast(Pinned, Message);
    }
  });
}

void FMcpBridgeWebSocket::ResetFragmentState() {
  FragmentAccumulator.Reset();
  bFragmentMessageActive = false;
}

bool FMcpBridgeWebSocket::ReceiveFrame() {
  uint8 Header[2];
  if (!ReceiveExact(Header, 2)) {
    TearDown(TEXT("Failed to read WebSocket frame header."), false, 4001);
    return false;
  }

  const bool bFinalFrame = (Header[0] & 0x80) != 0;
  const uint8 OpCode = Header[0] & 0x0F;
  uint64 PayloadLength = Header[1] & 0x7F;
  const bool bMasked = (Header[1] & 0x80) != 0;

  if (bServerAcceptedConnection && !bMasked) {
    TearDown(TEXT("Client frames must be masked."), false, 1002);
    return false;
  }

  if (PayloadLength == 126) {
    uint8 Extended[2];
    if (!ReceiveExact(Extended, sizeof(Extended))) {
      TearDown(TEXT("Failed to read extended payload length."), false, 4001);
      return false;
    }
    uint16 ShortVal = 0;
    FMemory::Memcpy(&ShortVal, Extended, sizeof(uint16));
    PayloadLength = FromNetwork16(ShortVal);
  } else if (PayloadLength == 127) {
    uint8 Extended[8];
    if (!ReceiveExact(Extended, sizeof(Extended))) {
      TearDown(TEXT("Failed to read extended payload length."), false, 4001);
      return false;
    }
    uint64 LongVal = 0;
    FMemory::Memcpy(&LongVal, Extended, sizeof(uint64));
    PayloadLength = FromNetwork64(LongVal);
  }

  if (PayloadLength > MaxWebSocketFramePayloadBytes) {
    TearDown(TEXT("WebSocket message too large."), false, WebSocketCloseCodeMessageTooBig);
    return false;
  }

  uint8 MaskKey[4] = {0, 0, 0, 0};
  if (bMasked) {
    if (!ReceiveExact(MaskKey, 4)) {
      TearDown(TEXT("Failed to read masking key."), false, 4001);
      return false;
    }
  }

  TArray<uint8> Payload;
  if (PayloadLength > 0) {
    Payload.SetNumUninitialized(static_cast<int32>(PayloadLength));
    if (!ReceiveExact(Payload.GetData(), PayloadLength)) {
      TearDown(TEXT("Failed to read WebSocket payload."), false, 4001);
      return false;
    }
    if (bMasked) {
      for (uint64 Index = 0; Index < PayloadLength; ++Index) {
        Payload[Index] ^= MaskKey[Index % 4];
      }
    }
  }

  if (OpCode == OpCodeClose) {
    TearDown(TEXT("WebSocket closed by peer."), true, 1000);
    return false;
  }

  // Handle control frames immediately (they must not be fragmented)
  if ((OpCode & 0x08) != 0) {
    if (!bFinalFrame) {
      TearDown(TEXT("Control frames must not be fragmented."), false, 4002);
      return false;
    }

    if (OpCode == OpCodePing) {
      SendControlFrame(OpCodePong, Payload);
      return true;
    }

    if (OpCode == OpCodePong) {
      // In server mode, receiving a pong means the client is responding to our
      // ping In client mode, receiving a pong means the server responded to our
      // ping
      HeartbeatDelegate.Broadcast(SelfWeakPtr.Pin());
      return true;
    }

    // Unknown control frame
    return true;
  }

  if (OpCode == OpCodeContinuation) {
    if (!bFragmentMessageActive) {
      TearDown(TEXT("Unexpected continuation frame."), false, 4002);
      return false;
    }

    const uint64 NewSize = static_cast<uint64>(FragmentAccumulator.Num()) + static_cast<uint64>(Payload.Num());
    if (NewSize > MaxWebSocketMessageBytes) {
      TearDown(TEXT("WebSocket message too large."), false, WebSocketCloseCodeMessageTooBig);
      return false;
    }

    FragmentAccumulator.Append(Payload);

    if (bFinalFrame) {
      HandleTextPayload(FragmentAccumulator);
      ResetFragmentState();
    }
    return true;
  }

  if (bFragmentMessageActive) {
    TearDown(
        TEXT("Received new data frame before completing fragmented message."),
        false, 4002);
    return false;
  }

  if (OpCode == OpCodeText) {
    if (bFinalFrame) {
      HandleTextPayload(Payload);
    } else {
      if (static_cast<uint64>(Payload.Num()) > MaxWebSocketMessageBytes) {
        TearDown(TEXT("WebSocket message too large."), false, WebSocketCloseCodeMessageTooBig);
        return false;
      }
      FragmentAccumulator = Payload;
      bFragmentMessageActive = true;
    }
    return true;
  }

  if (OpCode == OpCodeBinary) {
    TearDown(TEXT("Binary frames are not supported."), false, 4003);
    return false;
  }

  TearDown(TEXT("Unsupported WebSocket opcode."), false, 4003);
  return false;
}

bool FMcpBridgeWebSocket::ReceiveExact(uint8 *Buffer, SIZE_T Length) {
  SIZE_T Collected = 0;

  {
    FScopeLock Guard(&ReceiveMutex);
    const SIZE_T Existing =
        FMath::Min(static_cast<SIZE_T>(PendingReceived.Num()), Length);
    if (Existing > 0) {
      FMemory::Memcpy(Buffer, PendingReceived.GetData(), Existing);
      PendingReceived.RemoveAt(0, Existing
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4
      // UE 5.4+: Uses EAllowShrinking enum
      , EAllowShrinking::No
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      // UE 5.1-5.3: Uses bool bAllowShrinking
      , false
#endif
      );
      Collected += Existing;
    }
  }

  if (bUseTls && SslHandle) {
    while (Collected < Length) {
      if (bStopping) {
        return false;
      }

      const int32 Remaining = static_cast<int32>(Length - Collected);
      int32 BytesRead = 0;
      if (!RecvRaw(Buffer + Collected, Remaining, BytesRead)) {
        return false;
      }
      if (BytesRead <= 0) {
        if (StopEvent && StopEvent->Wait(FTimespan::FromMilliseconds(10))) {
          return false;
        }
        continue;
      }
      Collected += static_cast<SIZE_T>(BytesRead);
    }
    return true;
  }

  while (Collected < Length) {
    if (bStopping) {
      return false;
    }

    uint32 PendingSize = 0;
    if (!Socket->HasPendingData(PendingSize)) {
      if (StopEvent && StopEvent->Wait(FTimespan::FromMilliseconds(50))) {
        return false;
      }
      continue;
    }

    const uint32 ReadSize = FMath::Min<uint32>(PendingSize, 4096);
    TArray<uint8> Temp;
    Temp.SetNumUninitialized(ReadSize);
    int32 BytesRead = 0;
    if (!RecvRaw(Temp.GetData(), ReadSize, BytesRead)) {
      return false;
    }

    if (BytesRead <= 0) {
      continue;
    }

    const uint32 CopyCount =
        FMath::Min<uint32>(static_cast<uint32>(BytesRead),
                           static_cast<uint32>(Length - Collected));
    FMemory::Memcpy(Buffer + Collected, Temp.GetData(), CopyCount);
    Collected += CopyCount;

    if (static_cast<uint32>(BytesRead) > CopyCount) {
      FScopeLock Guard(&ReceiveMutex);
      PendingReceived.Append(Temp.GetData() + CopyCount, BytesRead - CopyCount);
    }
  }

  return true;
}
