#include "McpBridgeWebSocket.h"

#include "McpBridgeWebSocketPrivate.h"

#include "HAL/Event.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopeLock.h"
#include "Misc/Timespan.h"
#include "Sockets.h"

using namespace McpBridgeWebSocket;

void FMcpBridgeWebSocket::HandleTextPayload(const TArray<uint8> &Payload) {
  const FString Message = BytesToStringView(Payload);
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
    TearDown(TEXT("WebSocket message too large."), false,
             WebSocketCloseCodeMessageTooBig);
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
      HeartbeatDelegate.Broadcast(SelfWeakPtr.Pin());
      return true;
    }

    return true;
  }

  if (OpCode == OpCodeContinuation) {
    if (!bFragmentMessageActive) {
      TearDown(TEXT("Unexpected continuation frame."), false, 4002);
      return false;
    }

    const uint64 NewSize = static_cast<uint64>(FragmentAccumulator.Num()) +
                           static_cast<uint64>(Payload.Num());
    if (NewSize > MaxWebSocketMessageBytes) {
      TearDown(TEXT("WebSocket message too large."), false,
               WebSocketCloseCodeMessageTooBig);
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
    TearDown(TEXT("Received new data frame before completing fragmented message."),
             false, 4002);
    return false;
  }

  if (OpCode == OpCodeText) {
    if (bFinalFrame) {
      HandleTextPayload(Payload);
    } else {
      if (static_cast<uint64>(Payload.Num()) > MaxWebSocketMessageBytes) {
        TearDown(TEXT("WebSocket message too large."), false,
                 WebSocketCloseCodeMessageTooBig);
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
      , EAllowShrinking::No
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
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
