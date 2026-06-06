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

struct FParsedWebSocketUrl {
  FString Host;
  int32 Port = 80;
  FString PathWithQuery;
  bool bUseTls = false;
};

bool ParseWebSocketUrl(const FString &InUrl, FParsedWebSocketUrl &OutParsed,
                       FString &OutError);
uint16 ToNetwork16(uint16 Value);
uint16 FromNetwork16(uint16 Value);
uint64 ToNetwork64(uint64 Value);
uint64 FromNetwork64(uint64 Value);
FString BytesToStringView(const TArray<uint8> &Data);
void FillWebSocketRandomBytes(uint8 *Dest, int32 Count);
void DispatchOnGameThread(TFunction<void()> &&Fn);
FString DescribeSocketError(ISocketSubsystem *SocketSubsystem,
                            const TCHAR *Context);

}
