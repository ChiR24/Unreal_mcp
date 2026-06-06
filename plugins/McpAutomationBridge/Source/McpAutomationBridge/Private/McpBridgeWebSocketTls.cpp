#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "Ssl.h"
#include "SslModule.h"

#if WITH_SSL

#define UI OSSL_UI

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>

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
  if (SSL_CTX_use_certificate_file(SslContext, CertPathUtf8.Get(),
                                   SSL_FILETYPE_PEM) <= 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("Failed to load TLS certificate: %s"), *TlsCertificatePath);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(SslContext, KeyPathUtf8.Get(),
                                  SSL_FILETYPE_PEM) <= 0) {
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

#else

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

void FMcpBridgeWebSocket::ShutdownTls() {}

void FMcpBridgeWebSocket::CloseNativeSocket() {}

#endif
