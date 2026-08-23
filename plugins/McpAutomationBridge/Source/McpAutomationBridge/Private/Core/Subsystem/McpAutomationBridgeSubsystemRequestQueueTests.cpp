#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
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

namespace McpAutomationSaturationTestUtils
{
static const TMap<EMcpStateKind, int64> NoRevisions;
}  // namespace McpAutomationSaturationTestUtils

// BB-003 regression harness: saturate one session key, assert the precise
// refusal, drain admitted work at lane depth one, then accept a NEW request
// after recovery (plan Todo 8 acceptance, run on the game thread like the
// sibling RequestQueue tests).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpAutomationSaturationRecoveryTest,
    "McpAutomationBridge.Core.RequestQueue.SaturationRejectsPreciselyAndRecovers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpAutomationSaturationRecoveryTest::RunTest(const FString &Parameters) {
  (void)Parameters;
  using namespace McpAutomationSaturationTestUtils;
  UMcpAutomationBridgeSubsystem *Subsystem =
      NewObject<UMcpAutomationBridgeSubsystem>();
  Subsystem->AddToRoot();
  ON_SCOPE_EXIT { Subsystem->RemoveFromRoot(); };

  int32 DispatchCount = 0;
  TestTrue(
      TEXT("saturation test handler registered"),
      Subsystem->RegisterHandler(
          TEXT("saturation_fixture"),
          [&DispatchCount](
              const FString &, const FString &,
              const TSharedPtr<FJsonObject> &,
              TSharedPtr<FMcpBridgeWebSocket>) {
            ++DispatchCount;
            return true;
          }));

  // The queue accepts by default (bAcceptingAutomationRequests = true), so no
  // explicit StartAccepting call is needed — the ShutdownCancellation sibling
  // queues the same way without one.

  // The per-session cap equals the per-tick batch, so a full session is one
  // drain's worth of work and the batch never splits its admissions.
  constexpr int32 SessionCap =
      FMcpQueueFairnessState::MaxPendingRequestsPerSession;
  const FString SessionKey = TEXT("native:sessionA");
  for (int32 Index = 0; Index < SessionCap; ++Index)
  {
    TestEqual(
        TEXT("session request admitted up to the per-session cap"),
        Subsystem->QueueAutomationRequest(
            FString::Printf(TEXT("sat-%d"), Index), TEXT("saturation_fixture"),
            MakeShared<FJsonObject>(), nullptr, ERequestOrigin::NativeHTTP,
            NoRevisions, SessionKey),
        EAutomationQueueRejection::None);
  }

  // 17th request under the SAME session key: precise SessionQueueFull refusal
  // (the :67 path), never the generic global-cap code.
  TestEqual(
      TEXT("17th same-session request refused with SessionQueueFull"),
      Subsystem->QueueAutomationRequest(
          TEXT("sat-over"), TEXT("saturation_fixture"),
          MakeShared<FJsonObject>(), nullptr, ERequestOrigin::NativeHTTP,
          NoRevisions, SessionKey),
      EAutomationQueueRejection::SessionQueueFull);

  // Fairness preserved: a DIFFERENT session key is still admitted while the
  // first session is saturated (16 < global 64, per-key cap).
  TestEqual(
      TEXT("a different session is still admitted while sessionA is full"),
      Subsystem->QueueAutomationRequest(
          TEXT("other-0"), TEXT("saturation_fixture"),
          MakeShared<FJsonObject>(), nullptr, ERequestOrigin::NativeHTTP,
          NoRevisions, TEXT("native:sessionB")),
      EAutomationQueueRejection::None);

  // 17 admitted requests (16 sessionA + 1 sessionB) sit in the queue. The
  // per-tick batch is 16, so the FIRST drain dispatches exactly one batch and
  // leaves one admitted request queued; the queue is NOT empty until the
  // second drain. Asserting the residual is what accounts for every admitted
  // request: DispatchCount==16 proves the refused sat-over was never queued
  // or dispatched, Num()==1 proves the admitted sessionB request survives.
  Subsystem->ProcessPendingAutomationRequests();

  TestEqual(
      TEXT("first drain dispatches exactly one batch (16/tick) of admitted requests"),
      DispatchCount, SessionCap);
  TestEqual(
      TEXT("one admitted request remains queued after the first drain"),
      Subsystem->PendingAutomationRequests.Num(), 1);
  TestEqual(
      TEXT("the single game-thread lane never exceeded depth one"),
      Subsystem->QueueFairness.MaxObservedDispatchDepth, 1);
  TestEqual(
      TEXT("the lane depth unwinds to zero"),
      Subsystem->QueueFairness.DispatchDepth, 0);

  // SECOND drain, before any recovery admission: the surviving admitted
  // request dispatches, so all 17 admitted requests have drained exactly once
  // and the queue is empty. The refused sat-over request never entered the
  // queue, so it is not dispatched here either.
  Subsystem->ProcessPendingAutomationRequests();

  TestEqual(
      TEXT("second drain dispatches the surviving admitted request"),
      DispatchCount, SessionCap + 1);
  TestEqual(
      TEXT("all 17 admitted requests drained; the queue is empty"),
      Subsystem->PendingAutomationRequests.Num(), 0);
  TestEqual(
      TEXT("the single game-thread lane never exceeded depth one"),
      Subsystem->QueueFairness.MaxObservedDispatchDepth, 1);
  TestEqual(
      TEXT("the lane depth unwinds to zero"),
      Subsystem->QueueFairness.DispatchDepth, 0);

  TestEqual(
      TEXT("a new request is accepted after recovery"),
      Subsystem->QueueAutomationRequest(
          TEXT("sat-after"), TEXT("saturation_fixture"),
          MakeShared<FJsonObject>(), nullptr, ERequestOrigin::NativeHTTP,
          NoRevisions, SessionKey),
      EAutomationQueueRejection::None);
  // THIRD drain: the post-recovery request dispatches. DispatchCount==18
  // (16 sessionA + 1 sessionB + 1 sat-after) proves every admitted request was
  // dispatched exactly once and the refused sat-over was NEVER dispatched.
  Subsystem->ProcessPendingAutomationRequests();
  TestEqual(
      TEXT("the post-recovery request dispatched exactly once"),
      DispatchCount, SessionCap + 2);
  TestEqual(
      TEXT("the queue is empty after the post-recovery drain"),
      Subsystem->PendingAutomationRequests.Num(), 0);
  TestEqual(
      TEXT("the single game-thread lane never exceeded depth one"),
      Subsystem->QueueFairness.MaxObservedDispatchDepth, 1);
  TestEqual(
      TEXT("the lane depth unwinds to zero"),
      Subsystem->QueueFairness.DispatchDepth, 0);

  return true;
}
#endif
