#include "Transport/WebSocket/McpBridgeWebSocketPrivate.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "HAL/PlatformMisc.h"
#include "Math/UnrealMathUtility.h"
#include "SocketSubsystem.h"
#include "String/LexFromString.h"

namespace McpBridgeWebSocket {

FString BytesToStringView(const TArray<uint8> &Data) {
  if (Data.Num() == 0) {
    return FString();
  }
  const ANSICHAR *Utf8Ptr = reinterpret_cast<const ANSICHAR *>(Data.GetData());
  FUTF8ToTCHAR Converter(Utf8Ptr, Data.Num());
  if (Converter.Length() <= 0) {
    return FString();
  }
  return FString(Converter.Length(), Converter.Get());
}

void DispatchOnGameThread(TFunction<void()> &&Fn) {
  if (IsInGameThread()) {
    Fn();
    return;
  }

  AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
}

FString DescribeSocketError(ISocketSubsystem *SocketSubsystem,
                            const TCHAR *Context) {
  if (!SocketSubsystem) {
    return FString::Printf(TEXT("%s (no socket subsystem)"), Context);
  }

  const ESocketErrors LastErrorCode = SocketSubsystem->GetLastErrorCode();
  const FString Description = SocketSubsystem->GetSocketError(LastErrorCode);
  return FString::Printf(TEXT("%s (error=%d, %s)"), Context,
                         static_cast<int32>(LastErrorCode), *Description);
}

}
