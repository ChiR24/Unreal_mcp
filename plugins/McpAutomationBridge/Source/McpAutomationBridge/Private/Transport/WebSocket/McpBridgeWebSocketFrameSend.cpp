#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocketPrivate.h"

#include "Containers/StringConv.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ByteSwap.h"
#include "Misc/ScopeLock.h"

using namespace McpBridgeWebSocket;

namespace {
// FIN + OpCode, the payload length (7-bit, 16-bit or 64-bit form), then the payload.
// Server-to-client frames are never masked (RFC 6455 5.1).
TArray<uint8> BuildServerFrame(uint8 OpCode, const uint8 *Data, SIZE_T Length) {
  TArray<uint8> Frame;
  Frame.Add(0x80 | OpCode);
  if (Length <= 125) {
    Frame.Add(static_cast<uint8>(Length));
  } else if (Length <= 0xFFFF) {
    Frame.Add(126);
    const uint16 SizeShort = NETWORK_ORDER16(static_cast<uint16>(Length));
    Frame.Append(reinterpret_cast<const uint8 *>(&SizeShort), sizeof(uint16));
  } else {
    Frame.Add(127);
    const uint64 SizeLong = NETWORK_ORDER64(static_cast<uint64>(Length));
    Frame.Append(reinterpret_cast<const uint8 *>(&SizeLong), sizeof(uint64));
  }
  Frame.Append(Data, static_cast<int32>(Length));
  return Frame;
}
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
bool FMcpBridgeWebSocket::SendTextFrame(const void *Data, SIZE_T Length) {
  const TArray<uint8> Frame =
      BuildServerFrame(OpCodeText, static_cast<const uint8 *>(Data), Length);
  FScopeLock Guard(&SendMutex);
  return SendFrame(Frame);
}

bool FMcpBridgeWebSocket::SendControlFrame(const uint8 ControlOpCode,
                                           const TArray<uint8> &Payload) {
  // A control frame carries at most 125 payload bytes (RFC 6455 5.5).
  if (Payload.Num() > 125) {
    return false;
  }
  const TArray<uint8> Frame =
      BuildServerFrame(ControlOpCode & 0x0F, Payload.GetData(), Payload.Num());
  FScopeLock Guard(&SendMutex);
  return SendFrame(Frame);
}

// Tells an upgraded peer why it is being closed (RFC 6455 5.5.1: a 2-byte code, then
// UTF-8 reason). Without it the 4004/4005/4008/4403 codes the bridge closes with never
// left the process and the client saw an abnormal 1006. Once per socket; best effort.
void FMcpBridgeWebSocket::SendCloseFrame(int32 StatusCode, const FString &Reason) {
  if (!bConnected || bCloseFrameSent.Exchange(true)) {
    return;
  }
  const uint16 Code = NETWORK_ORDER16(static_cast<uint16>(StatusCode));
  TArray<uint8> Payload(reinterpret_cast<const uint8 *>(&Code), sizeof(Code));
  const FTCHARToUTF8 ReasonUtf8(*Reason);
  Payload.Append(reinterpret_cast<const uint8 *>(ReasonUtf8.Get()), FMath::Min(ReasonUtf8.Length(), 123));
  SendControlFrame(OpCodeClose, Payload);
}
