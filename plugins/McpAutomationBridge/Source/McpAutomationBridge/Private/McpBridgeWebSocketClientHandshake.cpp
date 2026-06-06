#include "McpBridgeWebSocket.h"

#include "McpBridgeWebSocketPrivate.h"

#include "Containers/StringConv.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/StringBuilder.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "String/LexFromString.h"

using namespace McpBridgeWebSocket;

bool FMcpBridgeWebSocket::PerformHandshake() {
  FParsedWebSocketUrl ParsedUrl;
  FString ParseError;
  if (!ParseWebSocketUrl(Url, ParsedUrl, ParseError)) {
    TearDown(ParseError, false, WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  if (bUseTls && !ParsedUrl.bUseTls) {
    TearDown(TEXT("TLS is enabled but ws:// URL was provided."), false,
             WebSocketCloseCodeAbnormalClosure);
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
    TearDown(TEXT("Unable to resolve WebSocket host."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  ISocketSubsystem *SocketSubsystem =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  Socket = SocketSubsystem->CreateSocket(
      NAME_Stream, TEXT("McpAutomationBridgeSocket"), false);
  if (!Socket) {
    TearDown(TEXT("Failed to create socket."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }
  Socket->SetReuseAddr(true);
  Socket->SetNonBlocking(false);
  Socket->SetNoDelay(true);

  Endpoint->SetPort(Port);
  if (!Socket->Connect(*Endpoint)) {
    TearDown(TEXT("Unable to connect to WebSocket endpoint."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  if (bUseTls) {
    if (!EstablishTls(false)) {
      TearDown(TEXT("TLS handshake failed."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }
  }

  TArray<uint8> KeyBytes;
  KeyBytes.SetNumUninitialized(16);
  FillWebSocketRandomBytes(KeyBytes.GetData(), KeyBytes.Num());
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
    TearDown(TEXT("Failed to send WebSocket handshake."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  TArray<uint8> ResponseBuffer;
  ResponseBuffer.Reserve(512);
  constexpr int32 TempSize = 256;
  constexpr int32 MaxHandshakeHeaderBytes = 8192;
  uint8 Temp[TempSize];
  bool bHandshakeComplete = false;
  int32 HeaderEndIndex = -1;
  while (!bHandshakeComplete) {
    if (bStopping) {
      return false;
    }
    int32 BytesRead = 0;
    if (!RecvRaw(Temp, TempSize, BytesRead)) {
      TearDown(TEXT("WebSocket handshake failed while reading response."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }
    if (BytesRead <= 0) {
      continue;
    }
    ResponseBuffer.Append(Temp, BytesRead);
    if (ResponseBuffer.Num() >= 4) {
      for (int32 Idx = 0; Idx + 3 < ResponseBuffer.Num(); ++Idx) {
        if (ResponseBuffer[Idx] == '\r' && ResponseBuffer[Idx + 1] == '\n' &&
            ResponseBuffer[Idx + 2] == '\r' &&
            ResponseBuffer[Idx + 3] == '\n') {
          HeaderEndIndex = Idx + 4;
          bHandshakeComplete = true;
          break;
        }
      }
    }
    if (bHandshakeComplete) {
      if (HeaderEndIndex > MaxHandshakeHeaderBytes) {
        TearDown(TEXT("WebSocket handshake response too large."), false,
                 WebSocketCloseCodeAbnormalClosure);
        return false;
      }
    } else if (ResponseBuffer.Num() > MaxHandshakeHeaderBytes) {
      TearDown(TEXT("WebSocket handshake response too large."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }
  }

  TArray<uint8> HeaderBytes;
  HeaderBytes.Append(ResponseBuffer.GetData(), HeaderEndIndex);
  FString HeaderSection = BytesToStringView(HeaderBytes);

  TArray<FString> HeaderLines;
  HeaderSection.ParseIntoArrayLines(HeaderLines, false);
  if (HeaderLines.Num() == 0) {
    TearDown(TEXT("Malformed WebSocket handshake response."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  const FString &StatusLine = HeaderLines[0];
  TArray<FString> StatusParts;
  StatusLine.ParseIntoArrayWS(StatusParts);
  int32 StatusCode = 0;
  if (StatusParts.Num() < 2 || !LexTryParseString(StatusCode, *StatusParts[1]) ||
      StatusCode != 101) {
    TearDown(TEXT("WebSocket server rejected handshake."), false,
             WebSocketCloseCodeAbnormalClosure);
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
    TearDown(TEXT("WebSocket handshake validation failed."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  if (HeaderEndIndex > 0 && HeaderEndIndex < ResponseBuffer.Num()) {
    PendingReceived.Append(ResponseBuffer.GetData() + HeaderEndIndex,
                           ResponseBuffer.Num() - HeaderEndIndex);
  }

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
