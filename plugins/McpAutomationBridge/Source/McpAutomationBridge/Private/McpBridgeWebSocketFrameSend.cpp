#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "Containers/StringConv.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopeLock.h"

using namespace McpBridgeWebSocket;

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
  const int32 ReasonBytes = FMath::Min<int32>(ReasonUtf8.Length(), 123);
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
    FillWebSocketRandomBytes(MaskKey, UE_ARRAY_COUNT(MaskKey));
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
    FillWebSocketRandomBytes(MaskKey, UE_ARRAY_COUNT(MaskKey));

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
