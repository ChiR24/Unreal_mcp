#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "McpConnectionManager.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpAutomationShutdownCancellationTest,
    "McpAutomationBridge.Core.RequestQueue.ShutdownCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpAutomationShutdownCancellationTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  UMcpAutomationBridgeSubsystem *Subsystem =
      NewObject<UMcpAutomationBridgeSubsystem>();
  int32 DispatchCount = 0;
  bool bCancellationCalled = false;
  TestTrue(
      TEXT("test handler registered"),
      Subsystem->RegisterHandler(
          TEXT("shutdown_cancellation_test"),
          [&DispatchCount](
              const FString &, const FString &,
              const TSharedPtr<FJsonObject> &,
              TSharedPtr<FMcpBridgeWebSocket>) {
            ++DispatchCount;
            return true;
          }));
  TestEqual(
      TEXT("queued request accepted"),
      Subsystem->QueueAutomationRequest(
          TEXT("queued-shutdown-request"),
          TEXT("shutdown_cancellation_test"),
          MakeShared<FJsonObject>(), nullptr),
      EAutomationQueueRejection::None);
  TestTrue(
      TEXT("asynchronous cancellation registered"),
      Subsystem->RegisterAutomationRequestCancellation(
          TEXT("async-shutdown-request"),
          [&bCancellationCalled]() { bCancellationCalled = true; }));

  Subsystem->StopAcceptingAutomationRequests();
  TestEqual(
      TEXT("late request admission is rejected"),
      Subsystem->QueueAutomationRequest(
          TEXT("late-shutdown-request"),
          TEXT("shutdown_cancellation_test"),
          MakeShared<FJsonObject>(), nullptr),
      EAutomationQueueRejection::NotAccepting);
  TestTrue(TEXT("shutdown cancellation finds outstanding work"),
           Subsystem->CancelAllAutomationRequests());
  Subsystem->ProcessPendingAutomationRequests();

  TestEqual(TEXT("queued request is not dispatched"), DispatchCount, 0);
  TestTrue(TEXT("asynchronous cancellation callback runs"),
           bCancellationCalled);
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpCancelScopeTest,
    "McpAutomationBridge.Core.ConnectionManager.CancelScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpCancelScopeTest::RunTest(const FString &Parameters) {
  (void)Parameters;
  FMcpConnectionManager Manager;

  TSharedPtr<FMcpBridgeWebSocket> Owner = MakeShared<FMcpBridgeWebSocket>(0);
  TSharedPtr<FMcpBridgeWebSocket> Intruder = MakeShared<FMcpBridgeWebSocket>(0);

  // Record req-1 as owned by Owner and authenticate both sockets so the
  // bridge_hello handshake gate passes.
  Manager.RegisterRequestSocket(TEXT("req-1"), Owner);
  Manager.AuthenticatedSockets.Add(Owner.Get());
  Manager.AuthenticatedSockets.Add(Intruder.Get());

  int32 CancelledCount = 0;
  FString LastCancelledRequestId;
  Manager.SetOnAutomationRequestCancelled(
      FMcpRequestCancelledCallback::CreateLambda(
          [&](const FString &RequestId) {
            ++CancelledCount;
            LastCancelledRequestId = RequestId;
          }));

  // Owner cancels its own tracked request: must forward to the subsystem.
  Manager.HandleCancelRequest(Owner, TEXT("req-1"));
  TestEqual(TEXT("owner cancel is forwarded to the subsystem"),
            CancelledCount, 1);
  TestEqual(TEXT("owner cancel forwards the correct requestId"),
            LastCancelledRequestId, TEXT("req-1"));

  // A different authenticated socket must NOT cancel a tracked request it
  // does not own.
  Manager.HandleCancelRequest(Intruder, TEXT("req-1"));
  TestEqual(TEXT("non-owner cancel is rejected (not forwarded)"),
            CancelledCount, 1);

  // Untracked (legacy/untracked) request: forwarded for backward compatibility.
  CancelledCount = 0;
  Manager.HandleCancelRequest(Owner, TEXT("unknown-req"));
  TestEqual(TEXT("untracked cancel is forwarded for backward compatibility"),
            CancelledCount, 1);

  // Unauthenticated socket: rejected before any scoping decision.
  TSharedPtr<FMcpBridgeWebSocket> Stranger = MakeShared<FMcpBridgeWebSocket>(0);
  CancelledCount = 0;
  Manager.HandleCancelRequest(Stranger, TEXT("unknown-req"));
  TestEqual(TEXT("unauthenticated cancel is rejected"), CancelledCount, 0);

  // Ownership lifecycle: closing a socket must purge all of its in-flight
  // request mappings so a later socket reusing the same raw address cannot
  // inherit ownership of a stale request.
  Manager.RegisterRequestSocket(TEXT("req-owner-close"), Owner);
  Manager.HandleClosed(Owner, 1000, TEXT("closed-for-test"), true);
  TestFalse(
      TEXT("closing owner purges its pending request mapping"),
      Manager.PendingRequestsToSockets.Contains(TEXT("req-owner-close")));

  // A cancel for the now-orphaned request, arriving from a different
  // authenticated socket, must be treated as untracked (forwarded), never
  // attributed to the stale (closed) owner.
  CancelledCount = 0;
  Manager.HandleCancelRequest(Intruder, TEXT("req-owner-close"));
  TestEqual(
      TEXT("cancel for closed-owner request is untracked (forwarded)"),
      CancelledCount, 1);

  return true;
}
#endif
