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
inline constexpr int32 WebSocketCloseCodeAbnormalClosure = 4000;
inline constexpr int32 WebSocketCloseCodeMessageTooBig = 1009;

FString BytesToStringView(const TArray<uint8> &Data);
void DispatchOnGameThread(TFunction<void()> &&Fn);
FString DescribeSocketError(ISocketSubsystem *SocketSubsystem,
                            const TCHAR *Context);

}
