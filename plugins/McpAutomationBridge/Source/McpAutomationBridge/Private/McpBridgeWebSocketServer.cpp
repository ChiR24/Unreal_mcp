#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

using namespace McpBridgeWebSocket;

uint32 FMcpBridgeWebSocket::RunServer() {
  const bool bIsIpv6Host = ListenHost.Contains(TEXT(":"));

  ISocketSubsystem *SocketSubsystem =
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("FMcpBridgeWebSocket::RunServer begin (host=%s, port=%d, IPv6=%s)"),
         *ListenHost, Port, bIsIpv6Host ? TEXT("true") : TEXT("false"));

  const FName ProtocolName = bIsIpv6Host ? FName(TEXT("IPv6")) : FName();
  ListenSocket = SocketSubsystem->CreateSocket(
      NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), ProtocolName);
  if (!ListenSocket) {
    const FString ErrorMessage = DescribeSocketError(
        SocketSubsystem, TEXT("Failed to create listen socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }
  ON_SCOPE_EXIT { DestroyListenSocket(); };

  ListenSocket->SetReuseAddr(true);
  ListenSocket->SetNonBlocking(false);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Listen socket created."));

  TSharedRef<FInternetAddr> ListenAddr = SocketSubsystem->CreateInternetAddr();
  bool bResolvedHost = false;

  FString HostToBind = ListenHost.TrimStartAndEnd();
  if (HostToBind.IsEmpty()) {
    HostToBind = TEXT("127.0.0.1");
  }

  if (HostToBind.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)) {
    HostToBind = TEXT("127.0.0.1");
  }

  const bool bIsLoopback =
      HostToBind.Equals(TEXT("127.0.0.1"), ESearchCase::IgnoreCase) ||
      HostToBind.Equals(TEXT("::1"), ESearchCase::IgnoreCase);

  const UMcpAutomationBridgeSettings *Settings =
      GetDefault<UMcpAutomationBridgeSettings>();
  const bool bAllowNonLoopback = Settings ? Settings->bAllowNonLoopback : false;

  if (bIsLoopback) {
    bool bIsValidIp = false;
    ListenAddr->SetIp(*HostToBind, bIsValidIp);
    bResolvedHost = bIsValidIp;

    if (!bResolvedHost && HostToBind.Equals(TEXT("::1"), ESearchCase::IgnoreCase)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("IPv6 loopback '::1' not supported on this system. Falling back to 127.0.0.1."));

      SocketSubsystem->DestroySocket(ListenSocket);
      ListenSocket = SocketSubsystem->CreateSocket(
          NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), FName());
      if (!ListenSocket) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to re-create IPv4 socket for fallback."));
        return 0;
      }
      ListenSocket->SetReuseAddr(true);
      ListenSocket->SetNonBlocking(false);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Re-created socket for IPv4 fallback."));

      bool bFallbackIsValidIp = false;
      ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
      bResolvedHost = bFallbackIsValidIp;
    }
  } else if (bAllowNonLoopback) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SECURITY: Binding to non-loopback address '%s'. The automation bridge is exposed to your local network."),
           *HostToBind);

    bool bIsValidIp = false;
    ListenAddr->SetIp(*HostToBind, bIsValidIp);
    bResolvedHost = bIsValidIp;

    if (!bResolvedHost) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("'%s' is not a valid IP address. Attempting DNS resolution..."),
             *HostToBind);

      FAddressInfoResult AddrInfoResult = SocketSubsystem->GetAddressInfo(
          *HostToBind, nullptr, EAddressInfoFlags::Default, NAME_None,
          ESocketType::SOCKTYPE_Streaming);

      if (AddrInfoResult.ReturnCode == SE_NO_ERROR &&
          AddrInfoResult.Results.Num() > 0) {
        ListenAddr = AddrInfoResult.Results[0].Address;
        bResolvedHost = true;
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Successfully resolved '%s' to address '%s'."), *HostToBind,
               *ListenAddr->ToString(true));

        const FName ProtocolType = ListenAddr->GetProtocolType();
        const bool bResolvedIsIpv6 = (ProtocolType == FName(TEXT("IPv6")));
        if (bResolvedIsIpv6 != bIsIpv6Host) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                 TEXT("DNS resolved to %s but socket is %s. Recreating socket..."),
                 bResolvedIsIpv6 ? TEXT("IPv6") : TEXT("IPv4"),
                 bIsIpv6Host ? TEXT("IPv6") : TEXT("IPv4"));

          SocketSubsystem->DestroySocket(ListenSocket);
          const FName NewProtocolName =
              bResolvedIsIpv6 ? FName(TEXT("IPv6")) : FName();
          ListenSocket = SocketSubsystem->CreateSocket(
              NAME_Stream, TEXT("McpAutomationBridgeListenSocket"),
              NewProtocolName);
          if (!ListenSocket) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                   TEXT("Failed to re-create socket for resolved address family."));
            return 0;
          }
          ListenSocket->SetReuseAddr(true);
          ListenSocket->SetNonBlocking(false);
        }
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to resolve hostname '%s'. Falling back to 127.0.0.1."),
               *HostToBind);
        bool bFallbackIsValidIp = false;
        ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
        bResolvedHost = bFallbackIsValidIp;
      }
    }
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("ListenHost '%s' is not a loopback address and bAllowNonLoopback is false. Falling back to 127.0.0.1. Enable 'Allow Non Loop Back' in Project Settings to use LAN addresses."),
           *HostToBind);

    if (bIsIpv6Host) {
      SocketSubsystem->DestroySocket(ListenSocket);
      ListenSocket = SocketSubsystem->CreateSocket(
          NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), FName());
      if (!ListenSocket) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to re-create IPv4 socket for fallback."));
        return 0;
      }
      ListenSocket->SetReuseAddr(true);
      ListenSocket->SetNonBlocking(false);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("Re-created socket for IPv4 fallback (non-loopback path)."));
    }

    bool bFallbackIsValidIp = false;
    ListenAddr->SetIp(TEXT("127.0.0.1"), bFallbackIsValidIp);
    bResolvedHost = bFallbackIsValidIp;
  }

  ListenAddr->SetPort(Port);

  if (!ListenSocket->Bind(*ListenAddr)) {
    const FString ErrorMessage =
        DescribeSocketError(SocketSubsystem, TEXT("Failed to bind listen socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Listen socket bound to %s."),
         *ListenAddr->ToString(false));

  if (!ListenSocket->Listen(ListenBacklog > 0 ? ListenBacklog : 10)) {
    const FString ErrorMessage =
        DescribeSocketError(SocketSubsystem, TEXT("Failed to listen on socket"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"), *ErrorMessage);
    DispatchOnGameThread([WeakThis = SelfWeakPtr, ErrorMessage] {
      if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
        Pinned->ConnectionErrorDelegate.Broadcast(ErrorMessage);
      }
    });
    return 0;
  }

  bListening = true;
  const FString BoundAddress = ListenAddr->ToString(false);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("MCP Automation Bridge listening on %s"), *BoundAddress);
  DispatchOnGameThread([WeakThis = SelfWeakPtr, BoundAddress] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      Pinned->ConnectedDelegate.Broadcast(Pinned);
    }
  });

  while (!bStopping && ListenSocket) {
    FSocket *ClientSocket =
        ListenSocket->Accept(TEXT("McpAutomationBridgeClient"));

    if (bStopping || !ListenSocket) {
      if (ClientSocket) {
        ClientSocket->Close();
        SocketSubsystem->DestroySocket(ClientSocket);
      }
      break;
    }

    if (ClientSocket) {
      HandleAcceptedClient(ClientSocket);
    } else {
      FPlatformProcess::Sleep(AcceptSleepSeconds > 0.0f ? AcceptSleepSeconds
                                                        : 0.01f);
    }
  }

  return 0;
}
