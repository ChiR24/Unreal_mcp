#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "EngineWireSubsystem.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Transport/WebSocket/McpBridgeWebSocketOpenSsl.h"

#if WITH_SSL

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

bool FMcpBridgeWebSocket::InitializeTlsContext() {
  if (SslContext) {
    return true;
  }

  // No call to the engine's SSL manager: it initialises OpenSSL only in a monolithic build and answers false in a
  // modular one, which the editor always is, so asking it refused every connection. This module links its own OpenSSL
  // (1.1.1 on every engine from 5.0), which initialises itself on first use.
  const SSL_METHOD *Method = TLS_server_method();
  if (!Method) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to resolve TLS server method."));
    return false;
  }

  SslContext = SSL_CTX_new(Method);
  if (!SslContext) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to create SSL server context."));
    return false;
  }

  SSL_CTX_set_min_proto_version(SslContext, TLS1_2_VERSION);
  SSL_CTX_set_options(SslContext, SSL_OP_NO_COMPRESSION);

  if (TlsCertificatePath.IsEmpty() || TlsPrivateKeyPath.IsEmpty()) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS is enabled but certificate or key path is missing."));
    return false;
  }

  if (!FPaths::FileExists(TlsCertificatePath)) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS certificate not found: %s"), *TlsCertificatePath);
    return false;
  }

  if (!FPaths::FileExists(TlsPrivateKeyPath)) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS private key not found: %s"), *TlsPrivateKeyPath);
    return false;
  }

  const FTCHARToUTF8 CertPathUtf8(*TlsCertificatePath);
  const FTCHARToUTF8 KeyPathUtf8(*TlsPrivateKeyPath);
  if (SSL_CTX_use_certificate_file(SslContext, CertPathUtf8.Get(),
                                   SSL_FILETYPE_PEM) <= 0) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to load TLS certificate: %s"), *TlsCertificatePath);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(SslContext, KeyPathUtf8.Get(),
                                  SSL_FILETYPE_PEM) <= 0) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to load TLS private key: %s"), *TlsPrivateKeyPath);
    return false;
  }
  if (SSL_CTX_check_private_key(SslContext) != 1) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS private key does not match certificate."));
    return false;
  }

  SSL_CTX_set_verify(SslContext, SSL_VERIFY_NONE, nullptr);
  return true;
}

bool FMcpBridgeWebSocket::EstablishTls() {
  if (!bUseTls) {
    return true;
  }

// TLS takes the OS socket over from the engine's FSocket, which can hand it out only where it has ReleaseNativeSocket.
#if MCP_HAS_SOCKET_RELEASE_NATIVE
  if (!Socket || bNativeSocketReleased) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS requested without a valid socket."));
    return false;
  }

  if (!InitializeTlsContext()) {
    return false;
  }

  NativeSocketHandle = Socket->ReleaseNativeSocket();
  bNativeSocketReleased = true;
  ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
  Socket = nullptr;

  if (NativeSocketHandle == 0) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to obtain native socket handle for TLS."));
    return false;
  }

  SslHandle = SSL_new(SslContext);
  if (!SslHandle) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("Failed to create SSL connection state."));
    return false;
  }

  SSL_set_fd(SslHandle, static_cast<int>(NativeSocketHandle));
  if (SSL_accept(SslHandle) <= 0) {
    UE_LOG(LogEngineWireSubsystem, Error, TEXT("TLS handshake failed."));
    return false;
  }

  return true;
#else
  UE_LOG(LogEngineWireSubsystem, Error,
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

  if (SslContext) {
    SSL_CTX_free(SslContext);
    SslContext = nullptr;
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

// Wakes a worker blocked in SSL_read on the native socket without freeing anything it is using.
void FMcpBridgeWebSocket::InterruptNativeSocket() {
  if (NativeSocketHandle == 0) {
    return;
  }
#if PLATFORM_WINDOWS
  shutdown(static_cast<SOCKET>(NativeSocketHandle), SD_BOTH);
#else
  shutdown(static_cast<int>(NativeSocketHandle), SHUT_RDWR);
#endif
}

#else

bool FMcpBridgeWebSocket::InitializeTlsContext() {
  if (bUseTls) {
    UE_LOG(LogEngineWireSubsystem, Warning,
           TEXT("TLS requested but WITH_SSL is not enabled."));
  }
  return !bUseTls;
}

bool FMcpBridgeWebSocket::EstablishTls() {
  if (bUseTls) {
    UE_LOG(LogEngineWireSubsystem, Error,
           TEXT("TLS requested but WITH_SSL is not enabled."));
    return false;
  }
  return true;
}

void FMcpBridgeWebSocket::ShutdownTls() {}

void FMcpBridgeWebSocket::CloseNativeSocket() {}

void FMcpBridgeWebSocket::InterruptNativeSocket() {}

#endif
