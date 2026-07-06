#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

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
#endif
