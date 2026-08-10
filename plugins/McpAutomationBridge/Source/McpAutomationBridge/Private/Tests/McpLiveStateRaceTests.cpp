#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Foundation/McpLiveStateRevisions.h"
#include "Misc/AutomationTest.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpLiveStateRaceBeforeDispatchTest,
	"McpAutomationBridge.Core.RequestQueue.StaleStateRaceBeforeDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpLiveStateRaceBeforeDispatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpLiveStateRevisions& Revisions = FMcpLiveStateRevisions::Get();
	Revisions.Reset();
	UMcpAutomationBridgeSubsystem* Subsystem =
		NewObject<UMcpAutomationBridgeSubsystem>();
	int32 DispatchCount = 0;
	TestTrue(TEXT("race fixture handler registered"),
		Subsystem->RegisterHandler(
			TEXT("task42_race_fixture"),
			[&DispatchCount](
				const FString&, const FString&, const TSharedPtr<FJsonObject>&,
				TSharedPtr<FMcpBridgeWebSocket>)
			{
				++DispatchCount;
				return true;
			}));

	FMcpExpectedRevisions Expected;
	Expected.Add(
		EMcpStateKind::Selection,
		Revisions.Current(EMcpStateKind::Selection));
	TestEqual(TEXT("race request accepted before state changes"),
		Subsystem->QueueAutomationRequest(
			TEXT("task42-race-request"), TEXT("task42_race_fixture"),
			MakeShared<FJsonObject>(), nullptr, ERequestOrigin::WebSocket, Expected),
		EAutomationQueueRejection::None);

	Revisions.Advance(EMcpStateKind::Selection);
	TestEqual(TEXT("race refusal uses the canonical code"),
		FString(FMcpLiveStateRevisions::StaleStateErrorCode()),
		FString(TEXT("STALE_STATE")));
	Subsystem->ProcessPendingAutomationRequests();
	TestEqual(TEXT("stale race invokes no handler"), DispatchCount, 0);
	return true;
}

#endif
