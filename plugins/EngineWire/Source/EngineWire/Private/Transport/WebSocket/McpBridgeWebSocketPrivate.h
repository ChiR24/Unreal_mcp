#pragma once

#include "CoreMinimal.h"

class ISocketSubsystem;

namespace McpBridgeWebSocket {

inline constexpr const TCHAR *WebSocketGuid =
    TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
inline constexpr uint8 OpCodeContinuation = 0x0;
inline constexpr uint8 OpCodeText = 0x1;
inline constexpr uint8 OpCodeBinary = 0x2;
inline constexpr uint8 OpCodeClose = 0x8;
inline constexpr uint8 OpCodePing = 0x9;
inline constexpr uint8 OpCodePong = 0xA;

inline constexpr uint64 MaxWebSocketMessageBytes = 5ULL * 1024ULL * 1024ULL;
inline constexpr uint64 MaxWebSocketFramePayloadBytes =
    MaxWebSocketMessageBytes;
// Connected clients one listener serves at once (each has a worker thread and a receive buffer).
inline constexpr int32 MaxWebSocketClients = 32;
// A frame's payload is read in pieces of this size, so a declared length reserves nothing that has not arrived.
inline constexpr uint64 WebSocketReceiveChunkBytes = 64ULL * 1024ULL;
inline constexpr int32 WebSocketCloseCodeAbnormalClosure = 4000;
inline constexpr int32 WebSocketCloseCodeMessageTooBig = 1009;

FString BytesToStringView(const TArray<uint8> &Data);
void DispatchOnGameThread(TFunction<void()> &&Fn);
FString DescribeSocketError(ISocketSubsystem *SocketSubsystem,
                            const TCHAR *Context);

}
