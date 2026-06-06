#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "Containers/StringConv.h"
#include "Misc/Base64.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"

using namespace McpBridgeWebSocket;

bool FMcpBridgeWebSocket::PerformServerHandshake() {
  TArray<uint8> RequestBuffer;
  RequestBuffer.Reserve(1024);
  constexpr int32 TempSize = 256;
  constexpr int32 MaxHandshakeHeaderBytes = 8192;
  uint8 Temp[TempSize];
  bool bRequestComplete = false;
  FString ClientKey;

  int32 HeaderEndIndex = -1;
  if (bUseTls) {
    if (!EstablishTls(true)) {
      TearDown(TEXT("TLS handshake failed."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }
  }

  while (!bRequestComplete) {
    if (bStopping) {
      return false;
    }

    int32 BytesRead = 0;
    if (!RecvRaw(Temp, TempSize, BytesRead)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Server handshake recv failed while awaiting upgrade request "
                  "(benign or client closed)."));
      TearDown(TEXT("Failed to read WebSocket upgrade request."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }

    if (BytesRead <= 0) {
      continue;
    }

    RequestBuffer.Append(Temp, BytesRead);

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
    if (bRequestComplete) {
      if (HeaderEndIndex > MaxHandshakeHeaderBytes) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("Server handshake upgrade request exceeded %d bytes."),
               MaxHandshakeHeaderBytes);
        TearDown(TEXT("WebSocket upgrade request too large."), false,
                 WebSocketCloseCodeAbnormalClosure);
        return false;
      }
    } else if (RequestBuffer.Num() > MaxHandshakeHeaderBytes) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("Server handshake upgrade request exceeded %d bytes."),
             MaxHandshakeHeaderBytes);
      TearDown(TEXT("WebSocket upgrade request too large."), false,
               WebSocketCloseCodeAbnormalClosure);
      return false;
    }
  }

  TArray<uint8> RequestHeaderBytes;
  RequestHeaderBytes.Append(RequestBuffer.GetData(), HeaderEndIndex);
  FString RequestString = BytesToStringView(RequestHeaderBytes);
  TArray<FString> RequestLines;
  RequestString.ParseIntoArrayLines(RequestLines, false);

  if (RequestLines.Num() == 0) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake received empty upgrade request."));
    TearDown(TEXT("Malformed WebSocket upgrade request."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  TArray<FString> RequestParts;
  RequestLines[0].ParseIntoArrayWS(RequestParts);
  if (RequestParts.Num() < 3 ||
      !RequestParts[0].Equals(TEXT("GET"), ESearchCase::IgnoreCase)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Server handshake received invalid request line: %s"),
           RequestLines.Num() > 0 ? *RequestLines[0] : TEXT("(empty)"));
    TearDown(TEXT("Invalid WebSocket upgrade request line."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

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
                 Value.ToLower().Contains(TEXT("upgrade"))) {
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
    TearDown(TEXT("Invalid WebSocket upgrade request."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  FString AcceptKey;
  {
    FTCHARToUTF8 AcceptUtf8(*(ClientKey + WebSocketGuid));
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
    TearDown(TEXT("Failed to send WebSocket upgrade response."), false,
             WebSocketCloseCodeAbnormalClosure);
    return false;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Server handshake completed; subprotocol=%s"),
         SelectedProtocol.IsEmpty() ? TEXT("(none)") : *SelectedProtocol);

  return true;
}
