#include "Core/Journal/McpChangeJournal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/Journal/McpChangeJournalState.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpChangeJournalValuesTest, "EngineWire.Core.ChangeJournal.Values",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A property's value before an edit announced the way the Details panel announces it (PreEditChange with its chain) and
// after it, on an asset made in memory for the test: never saved, and its row dropped from the journal again.
bool FMcpChangeJournalValuesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpChangeJournal;
    static const TCHAR* const Path = TEXT("/Game/_McpScratch/McpJournalValueTest");
    FJournalState& State = JournalState();
    const int32 Depth = State.RequestDepth;
    const FString RequestId = State.RequestId;
    const FString Action = State.Action;
    State.RequestDepth = 0;
    State.RequestId.Reset();
    State.Action.Reset();

    UPackage* Package = CreatePackage(Path);
    UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(Package, TEXT("McpJournalValueTest"), RF_Public);
    FProperty* Friction = FindFProperty<FProperty>(UPhysicalMaterial::StaticClass(), TEXT("Friction"));
    BeginRequest(TEXT("journal-values-test"), TEXT("inspect journal_values_test"));
    FEditPropertyChain Chain;
    Chain.AddHead(Friction);
    Chain.SetActivePropertyNode(Friction);
    Chain.SetActiveMemberPropertyNode(Friction);
    Material->PreEditChange(Chain);
    Material->Friction = 0.25f;
    FPropertyChangedEvent Event(Friction);
    Material->PostEditChangeProperty(Event);

    const FName FrictionName = Friction->GetFName();
    const auto Kept = [&State, &FrictionName]() -> const FJournalValue* {
        const FJournalEntry* Entry = State.Entries.FindByPredicate([](const FJournalEntry& Row) { return Row.Path == Path; });
        return Entry ? Entry->Values.FindByPredicate([&FrictionName](const FJournalValue& Row) { return Row.Property == FrictionName; }) : nullptr;
    };
    const FJournalValue* Value = Kept();
    TestTrue(TEXT("the edit is kept with the value it changed"), Value != nullptr);
    if (Value)
    {
        TestTrue(TEXT("from is the value before the edit"), Value->bFrom && FMath::IsNearlyEqual(FCString::Atof(*Value->From), 0.7f, 1e-4f));
        TestTrue(TEXT("to is the value after it"), Value->bTo && FMath::IsNearlyEqual(FCString::Atof(*Value->To), 0.25f, 1e-4f));
    }
    // A value equal to zeroed memory was exported as an empty string.
    Material->PreEditChange(Chain);
    Material->Friction = 0.0f;
    Material->PostEditChangeProperty(Event);
    Value = Kept();
    TestTrue(TEXT("a zero is written out, not left empty"), Value && !Value->To.IsEmpty() && FCString::Atof(*Value->To) == 0.0f);
    // An undo names no property: what is kept for the object is read again.
    Material->Friction = 0.5f;
    Material->PostEditChange();
    Value = Kept();
    TestTrue(TEXT("a change naming no property reads the kept values again"), Value && FMath::IsNearlyEqual(FCString::Atof(*Value->To), 0.5f, 1e-4f));
    TestTrue(TEXT("and keeps the first old value"), Value && FMath::IsNearlyEqual(FCString::Atof(*Value->From), 0.7f, 1e-4f));
    EndRequest();

    State.Entries.RemoveAll([](const FJournalEntry& Kept) { return Kept.Path == Path; });
    Reindex(State);
    Package->SetDirtyFlag(false);
    Material->MarkAsGarbage();
    Package->MarkAsGarbage();
    State.RequestDepth = Depth;
    State.RequestId = RequestId;
    State.Action = Action;
    return true;
}
#endif
