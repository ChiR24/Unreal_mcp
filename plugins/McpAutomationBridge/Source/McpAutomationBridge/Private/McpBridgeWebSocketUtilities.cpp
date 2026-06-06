#include "McpBridgeWebSocketPrivate.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "HAL/PlatformMisc.h"
#include "Math/UnrealMathUtility.h"
#include "SocketSubsystem.h"
#include "String/LexFromString.h"

namespace McpBridgeWebSocket {

bool ParseWebSocketUrl(const FString &InUrl, FParsedWebSocketUrl &OutParsed,
                       FString &OutError) {
  const FString Trimmed = InUrl.TrimStartAndEnd();
  if (Trimmed.IsEmpty()) {
    OutError = TEXT("WebSocket URL is empty.");
    return false;
  }

  static const FString SchemePrefix(TEXT("ws://"));
  static const FString SecureSchemePrefix(TEXT("wss://"));
  FString Remainder;
  if (Trimmed.StartsWith(SchemePrefix, ESearchCase::IgnoreCase)) {
    Remainder = Trimmed.Mid(SchemePrefix.Len());
    OutParsed.bUseTls = false;
  } else if (Trimmed.StartsWith(SecureSchemePrefix, ESearchCase::IgnoreCase)) {
    Remainder = Trimmed.Mid(SecureSchemePrefix.Len());
    OutParsed.bUseTls = true;
  } else {
    OutError = TEXT("Only ws:// or wss:// schemes are supported.");
    return false;
  }

  FString HostPort;
  FString PathRemainder;
  if (!Remainder.Split(TEXT("/"), &HostPort, &PathRemainder,
                       ESearchCase::CaseSensitive, ESearchDir::FromStart)) {
    HostPort = Remainder;
    PathRemainder.Reset();
  }

  HostPort = HostPort.TrimStartAndEnd();
  if (HostPort.IsEmpty()) {
    OutError = TEXT("WebSocket URL missing host.");
    return false;
  }

  FString Host;
  int32 Port = 80;

  if (HostPort.StartsWith(TEXT("["))) {
    int32 ClosingBracketIndex = INDEX_NONE;
    if (!HostPort.FindChar(TEXT(']'), ClosingBracketIndex)) {
      OutError = TEXT("Invalid IPv6 WebSocket host.");
      return false;
    }

    Host = HostPort.Mid(1, ClosingBracketIndex - 1);
    const int32 PortSeparatorIndex = HostPort.Find(
        TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (PortSeparatorIndex > ClosingBracketIndex) {
      const FString PortString = HostPort.Mid(PortSeparatorIndex + 1);
      if (!PortString.IsEmpty() && !LexTryParseString(Port, *PortString)) {
        OutError = TEXT("Invalid WebSocket port.");
        return false;
      }
    }
  } else {
    FString PortString;
    if (HostPort.Split(TEXT(":"), &Host, &PortString,
                       ESearchCase::CaseSensitive, ESearchDir::FromEnd)) {
      Host = Host.TrimStartAndEnd();
      PortString = PortString.TrimStartAndEnd();

      if (!PortString.IsEmpty()) {
        if (!LexTryParseString(Port, *PortString)) {
          OutError = TEXT("Invalid WebSocket port.");
          return false;
        }
      }
    } else {
      Host = HostPort;
    }
  }

  Host = Host.TrimStartAndEnd();
  if (Host.IsEmpty()) {
    OutError = TEXT("WebSocket URL missing host.");
    return false;
  }

  if (Port <= 0 || Port > 65535) {
    OutError = TEXT("WebSocket port must be between 1 and 65535.");
    return false;
  }

  FString PathWithQuery;
  if (PathRemainder.IsEmpty()) {
    PathWithQuery = TEXT("/");
  } else {
    PathWithQuery = TEXT("/") + PathRemainder;
  }

  OutParsed.Host = Host;
  OutParsed.Port = Port;
  OutParsed.PathWithQuery = PathWithQuery;
  return true;
}

uint16 ToNetwork16(uint16 Value) {
#if PLATFORM_LITTLE_ENDIAN
  return static_cast<uint16>(((Value & 0x00FF) << 8) | ((Value & 0xFF00) >> 8));
#else
  return Value;
#endif
}

uint16 FromNetwork16(uint16 Value) { return ToNetwork16(Value); }

uint64 ToNetwork64(uint64 Value) {
#if PLATFORM_LITTLE_ENDIAN
  return ((Value & 0x00000000000000FFULL) << 56) |
         ((Value & 0x000000000000FF00ULL) << 40) |
         ((Value & 0x0000000000FF0000ULL) << 24) |
         ((Value & 0x00000000FF000000ULL) << 8) |
         ((Value & 0x000000FF00000000ULL) >> 8) |
         ((Value & 0x0000FF0000000000ULL) >> 24) |
         ((Value & 0x00FF000000000000ULL) >> 40) |
         ((Value & 0xFF00000000000000ULL) >> 56);
#else
  return Value;
#endif
}

uint64 FromNetwork64(uint64 Value) { return ToNetwork64(Value); }

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

void FillWebSocketRandomBytes(uint8 *Dest, int32 Count) {
  int32 Offset = 0;
  while (Offset < Count) {
    FGuid Guid;
    FPlatformMisc::CreateGuid(Guid);
    const uint32 Words[4] = {Guid.A, Guid.B, Guid.C, Guid.D};
    const int32 CopyCount =
        FMath::Min(Count - Offset, static_cast<int32>(sizeof(Words)));
    FMemory::Memcpy(Dest + Offset, Words, CopyCount);
    Offset += CopyCount;
  }
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
