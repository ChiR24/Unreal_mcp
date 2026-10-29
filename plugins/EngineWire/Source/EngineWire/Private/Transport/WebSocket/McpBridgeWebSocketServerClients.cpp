#include "Transport/WebSocket/McpBridgeWebSocket.h"

#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocketPrivate.h"

#include "IPAddress.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

using namespace McpBridgeWebSocket;

void FMcpBridgeWebSocket::HandleAcceptedClient(FSocket* ClientSocket) {
  ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  TSharedRef<FInternetAddr> PeerAddr = SocketSubsystem->CreateInternetAddr();
  if (ClientSocket->GetPeerAddress(*PeerAddr)) {
    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("Accepted automation client from %s"),
           *PeerAddr->ToString(true));
  }

  auto ClientWebSocket = MakeShared<FMcpBridgeWebSocket>(
      ClientSocket, bUseTls, TlsCertificatePath, TlsPrivateKeyPath);
  ClientWebSocket->bServerMode = false;
  ClientWebSocket->bServerAcceptedConnection = true;
  ClientWebSocket->bListenerLoopback = bListenerLoopback;
  ClientWebSocket->Port = Port;

  {
    FScopeLock Lock(&ClientSocketsMutex);
    ClientSockets.Add(ClientWebSocket);
  }

  TWeakPtr<FMcpBridgeWebSocket> LocalWeakThis = AsWeak();
  // Weak: these callbacks are stored on the client itself, so a strong capture kept every closed client (and its
  // TLS state and native socket) alive for the life of the editor.
  TWeakPtr<FMcpBridgeWebSocket> WeakClient = ClientWebSocket;
  auto RemoveFromClientList = [LocalWeakThis, WeakClient] {
    const TSharedPtr<FMcpBridgeWebSocket> Client = WeakClient.Pin();
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin(); Pinned && Client) {
      FScopeLock Lock(&Pinned->ClientSocketsMutex);
      UE_LOG(LogEngineWireSubsystem, VeryVerbose,
             TEXT("Removing client socket from server tracking (remaining before remove: %d)."),
             Pinned->ClientSockets.Num());
      Pinned->ClientSockets.Remove(Client);
    }
  };

  // ConnectedDelegate is broadcast on the game thread already.
  ClientWebSocket->ConnectedDelegate.AddLambda(
      [LocalWeakThis](TSharedPtr<FMcpBridgeWebSocket> ClientSocket) {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = LocalWeakThis.Pin()) {
          UE_LOG(LogEngineWireSubsystem, Log,
                 TEXT("Broadcasting client connected delegate."));
          Pinned->ClientConnectedDelegate.Broadcast(ClientSocket);
        }
      });

  ClientWebSocket->ClosedDelegate.AddLambda(
      [RemoveFromClientList](TSharedPtr<FMcpBridgeWebSocket>, int32,
                             const FString&, bool) {
        RemoveFromClientList();
      });

  ClientWebSocket->ConnectionErrorDelegate.AddLambda(
      [RemoveFromClientList](const FString&) { RemoveFromClientList(); });

  ClientWebSocket->Connect();
}
