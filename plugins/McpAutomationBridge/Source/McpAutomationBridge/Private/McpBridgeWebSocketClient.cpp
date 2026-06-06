#include "McpBridgeWebSocket.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocketPrivate.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Timespan.h"

using namespace McpBridgeWebSocket;

uint32 FMcpBridgeWebSocket::RunClient() {
  if (bServerAcceptedConnection) {
    if (!PerformServerHandshake()) {
      return 0;
    }
  } else {
    if (!PerformHandshake()) {
      return 0;
    }
  }

  bConnected = true;
  UE_LOG(
      LogMcpAutomationBridgeSubsystem, Log,
      TEXT("FMcpBridgeWebSocket connection established (serverAccepted=%s)."),
      bServerAcceptedConnection ? TEXT("true") : TEXT("false"));
  DispatchOnGameThread([WeakThis = SelfWeakPtr] {
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin()) {
      Pinned->ConnectedDelegate.Broadcast(Pinned);
    }
  });

  if (bServerAcceptedConnection) {
    if (!HandlerReadyEvent) {
      HandlerReadyEvent = FPlatformProcess::GetSynchEventFromPool(true);
    }

    constexpr double MaxWaitSeconds = 0.5;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Awaiting message handler registration for new client "
                "connection (max %.0f ms)."),
           MaxWaitSeconds * 1000.0);
    HandlerReadyEvent->Wait(FTimespan::FromSeconds(MaxWaitSeconds));
    if (!bHandlerRegistered) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("Message handler registration not observed in time; "
                  "proceeding without explicit synchronization."));
    }
  }

  while (!bStopping) {
    if (!ReceiveFrame()) {
      break;
    }
  }

  TearDown(TEXT("Socket loop finished."), true, 1000);
  return 0;
}
