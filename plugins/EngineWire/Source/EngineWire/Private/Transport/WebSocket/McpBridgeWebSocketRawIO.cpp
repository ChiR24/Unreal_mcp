#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "Sockets.h"

#if WITH_SSL
#define UI OSSL_UI
#include <openssl/err.h>
#include <openssl/ssl.h>
#undef UI
#endif

bool FMcpBridgeWebSocket::SendRaw(const uint8* Data, int32 Length,
                                  int32& OutBytesSent) {
  OutBytesSent = 0;
#if WITH_SSL
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
#endif

  if (!Socket) {
    return false;
  }

  return Socket->Send(Data, Length, OutBytesSent);
}

bool FMcpBridgeWebSocket::RecvRaw(uint8* Data, int32 Length,
                                  int32& OutBytesRead) {
  OutBytesRead = 0;
#if WITH_SSL
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
#endif

  if (!Socket) {
    return false;
  }

  return Socket->Recv(Data, Length, OutBytesRead);
}
