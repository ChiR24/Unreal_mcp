#include "Core/Requests/McpUndoLedger.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"

namespace
{
const FName LedgerTag(TEXT("McpUndoLedgerProbe"));

/** The undo block Describe leaves on a reply that had none (or the handler's own), and its reasonCode. */
FString UndoVerdict(const TSharedPtr<FJsonObject>& Result)
{
    const TSharedPtr<FJsonObject>* Undo = nullptr;
    if (!Result->TryGetObjectField(TEXT("undo"), Undo) || !Undo)
    {
        return TEXT("none");
    }
    bool bUndoable = false;
    (*Undo)->TryGetBoolField(TEXT("undoable"), bUndoable);
    return bUndoable ? TEXT("undoable") : (*Undo)->GetStringField(TEXT("reasonCode"));
}

TSharedPtr<FJsonObject> Claimed()
{
    const TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject> Undo = MakeShared<FJsonObject>();
    Undo->SetBoolField(TEXT("undoable"), true);
    Undo->SetStringField(TEXT("transactionScope"), TEXT("Mcp Ledger"));
    Result->SetObjectField(TEXT("undo"), Undo);
    return Result;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpUndoLedgerTest, "EngineWire.Core.UndoLedger.ReadsTheStack",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpUndoLedgerTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    if (!GEditor || !GEditor->Trans)
    {
        AddError(TEXT("No editor transaction buffer; the ledger cannot be proven here."));
        return false;
    }
    GEditor->Trans->Reset(FText::FromString(TEXT("Mcp undo ledger test")));
    USceneComponent* Probe = NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transactional);

    // A step that changed the probe is kept, and a reply with no undo block is given one.
    McpUndoLedger::Begin(TEXT("ledger-kept"));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp Ledger")));
        Probe->Modify();
        Probe->ComponentTags.Add(LedgerTag);
    }
    TSharedPtr<FJsonObject> Silent = MakeShared<FJsonObject>();
    McpUndoLedger::Describe(TEXT("ledger-kept"), Silent);
    McpUndoLedger::End();
    TestEqual(TEXT("a kept step is reported"), UndoVerdict(Silent), FString(TEXT("undoable")));

    // A step that changed nothing is popped by the buffer: the handler's claim is corrected.
    McpUndoLedger::Begin(TEXT("ledger-noop"));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp Ledger")));
        Probe->Modify();
    }
    TSharedPtr<FJsonObject> NoOp = Claimed();
    McpUndoLedger::Describe(TEXT("ledger-noop"), NoOp);
    McpUndoLedger::End();
    TestEqual(TEXT("a claim the stack does not back is corrected"), UndoVerdict(NoOp), FString(TEXT("NOTHING_CHANGED")));

    // A request that clears the history says so, even when its handler said nothing.
    McpUndoLedger::Begin(TEXT("ledger-cleared"));
    GEditor->Trans->Reset(FText::FromString(TEXT("Mcp undo ledger test")));
    TSharedPtr<FJsonObject> Cleared = MakeShared<FJsonObject>();
    McpUndoLedger::Describe(TEXT("ledger-cleared"), Cleared);
    McpUndoLedger::End();
    TestEqual(TEXT("a cleared history is reported"), UndoVerdict(Cleared), FString(TEXT("UNDO_HISTORY_CLEARED")));

    // While a transaction is still open nothing is known yet: the handler's word stands, and silence stays silence.
    McpUndoLedger::Begin(TEXT("ledger-open"));
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp Ledger")));
        Probe->Modify();
        Probe->ComponentTags.Reset();
        TSharedPtr<FJsonObject> Open = MakeShared<FJsonObject>();
        McpUndoLedger::Describe(TEXT("ledger-open"), Open);
        TestEqual(TEXT("an open transaction is not judged"), UndoVerdict(Open), FString(TEXT("none")));
    }
    McpUndoLedger::End();

    // Most handlers reply inside their open transaction: that reply is held, and sent once it closed with what it kept.
    McpUndoLedger::Begin(TEXT("ledger-held"));
    TSharedPtr<FJsonObject> Held = MakeShared<FJsonObject>();
    bool bSent = false;
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("Mcp Ledger")));
        Probe->Modify();
        Probe->ComponentTags.Add(LedgerTag);
        TestTrue(TEXT("a reply inside an open transaction is held"), McpUndoLedger::Hold(TEXT("ledger-held"), [&Held, &bSent]()
        {
            McpUndoLedger::Describe(TEXT("ledger-held"), Held);
            bSent = true;
        }));
        TestFalse(TEXT("another request's reply is not held"), McpUndoLedger::Hold(TEXT("ledger-other"), []() {}));
    }
    TestFalse(TEXT("nothing is sent before the flush"), bSent);
    McpUndoLedger::Flush();
    McpUndoLedger::End();
    TestTrue(TEXT("the flush sends it"), bSent);
    TestEqual(TEXT("with the step its closed transaction kept"), UndoVerdict(Held), FString(TEXT("undoable")));

    // Another request's reply is never touched.
    TSharedPtr<FJsonObject> Other = MakeShared<FJsonObject>();
    McpUndoLedger::Describe(TEXT("ledger-other"), Other);
    TestEqual(TEXT("only the recorded request is described"), UndoVerdict(Other), FString(TEXT("none")));

    GEditor->Trans->Reset(FText::FromString(TEXT("Mcp undo ledger test")));
    return true;
}
#endif
