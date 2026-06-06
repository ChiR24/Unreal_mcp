#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "IPAddress.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

using namespace McpBridgeWebSocket;

void FMcpBridgeWebSocket::HandleAcceptedClient(FSocket* ClientSocket) {
  ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  TSharedRef<FInternetAddr> PeerAddr = SocketSubsystem->CreateInternetAddr();
  if (ClientSocket->GetPeerAddress(*PeerAddr)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("Accepted automation client from %s"),
           *PeerAddr->ToString(true));
  }

  auto ClientWebSocket = MakeShared<FMcpBridgeWebSocket>(
      ClientSocket, bUseTls, TlsCertificatePath, TlsPrivateKeyPath);
  ClientWebSocket->InitializeWeakSelf(ClientWebSocket);
  ClientWebSocket->bServerMode = false;
  ClientWebSocket->bServerAcceptedConnection = true;
  ClientWebSocket->Port = Port;

  {
    FScopeLock Lock(&ClientSocketsMutex);
    ClientSockets.Add(ClientWebSocket);
  }

  TWeakPtr<FMcpBridgeWebSocket> LocalWeakThis = SelfWeakPtr;
  auto RemoveFromClientList = [LocalWeakThis, ClientWebSocket] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin()) {
      FScopeLock Lock(&Pinned->ClientSocketsMutex);
      UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
             TEXT("Removing client socket from server tracking (remaining before remove: %d)."),
             Pinned->ClientSockets.Num());
      Pinned->ClientSockets.Remove(ClientWebSocket);
    }
  };

  ClientWebSocket->OnConnected().AddLambda(
      [LocalWeakThis, ClientWebSocket](TSharedPtr<FMcpBridgeWebSocket>) {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin()) {
          DispatchOnGameThread(
              [ParentWeak = LocalWeakThis, ClientSocket = ClientWebSocket] {
                if (TSharedPtr<FMcpBridgeWebSocket> DispatchPinned = ParentWeak.Pin()) {
                  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
                         TEXT("Broadcasting client connected delegate."));
                  DispatchPinned->ClientConnectedDelegate.Broadcast(ClientSocket);
                }
              });
        }
      });

  ClientWebSocket->OnClosed().AddLambda(
      [RemoveFromClientList](TSharedPtr<FMcpBridgeWebSocket>, int32,
                             const FString&, bool) {
        RemoveFromClientList();
      });

  ClientWebSocket->OnConnectionError().AddLambda(
      [RemoveFromClientList](const FString&) { RemoveFromClientList(); });

  ClientWebSocket->Connect();
}
