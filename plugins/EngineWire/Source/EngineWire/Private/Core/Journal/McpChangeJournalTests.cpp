#include "Core/Journal/McpChangeJournal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Journal/McpChangeJournalState.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpChangeJournalTest, "EngineWire.Core.ChangeJournal.Attribution",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Who a change is by, with nothing in the level touched: the test runs inside the request that started it, so that
// request's place in the journal is set aside first and put back after. The transactions change nothing, so the buffer
// drops each as it closes and the undo history stays as it was.
bool FMcpChangeJournalTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpChangeJournal;
    FJournalState& State = JournalState();
    const int32 Depth = State.RequestDepth;
    const FString RequestId = State.RequestId;
    const FString Action = State.Action;
    State.RequestDepth = 0;
    State.RequestId.Reset();
    State.Action.Reset();

    TestEqual(TEXT("with no request and no transaction, a change is the editor's"), int32(CurrentBy()), int32(JournalByEditor));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp journal test")));
        TestEqual(TEXT("inside a transaction no request opened, it is the user's"), int32(CurrentBy()), int32(JournalByUser));
    }
    BeginRequest(TEXT("journal-test"), TEXT("inspect journal_test"));
    TestEqual(TEXT("inside a request, it is this server's"), int32(CurrentBy()), int32(JournalByMcp));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp journal test")));
        TestEqual(TEXT("a request's own transaction is this server's too"), int32(CurrentBy()), int32(JournalByMcp));
    }
    BeginRequest(TEXT("journal-nested"), TEXT("inspect nested"));
    EndRequest();
    TestEqual(TEXT("a nested request ending leaves the outer one open"), State.RequestId, FString(TEXT("journal-test")));
    EndRequest();
    TestEqual(TEXT("after the request, it is the editor's again"), int32(CurrentBy()), int32(JournalByEditor));
    EndRequest();
    TestEqual(TEXT("an unmatched end changes nothing"), State.RequestDepth, 0);

    State.RequestDepth = Depth;
    State.RequestId = RequestId;
    State.Action = Action;
    return true;
}
#endif
