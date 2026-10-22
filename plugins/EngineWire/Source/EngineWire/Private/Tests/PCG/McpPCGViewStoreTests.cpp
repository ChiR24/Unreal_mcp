#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataViews.h"
#include "Tests/PCG/McpPCGRootFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "UObject/GarbageCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootRetentionTest, "EngineWire.PCG.RootData.BoundedRetentionAndRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootRetentionTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        using namespace McpPCGHandlers;
        UPCGComponent* Component = Fixture.GetComponent();
        FPCGDataViewStore Store;
        FString Error;
        const FString Root = Store.RetainRoot(Component, TEXT("owner"), 0, Error);
        if (!TestFalse(TEXT("Owned real root handle exists"), Root.IsEmpty())) return true;
        auto Value = MakeShared<FJsonObject>();
        Value->SetNumberField(TEXT("exact"), 1.2345678901234567);
        const TArray<TSharedPtr<FJsonValue>> Rows{MakeShared<FJsonValueObject>(Value)};
        const FString View = Store.RetainRows(Root, Component, TEXT("owner"), Rows, Error);
        if (!TestFalse(TEXT("Bounded immutable row view exists"), View.IsEmpty())) return true;
        Value->SetNumberField(TEXT("exact"), 9);
        TArray<TSharedPtr<FJsonValue>> Page;
        int32 Total = -1;
        TestTrue(TEXT("View reads captured rows"), Store.ReadRows(View, Component, TEXT("owner"), 0, 1, Page, Total, Error));
        if (!TestEqual(TEXT("One row is captured"), Page.Num(), 1)) return true;
        TestEqual(TEXT("Captured precision is immutable"), Page[0]->AsObject()->GetNumberField(TEXT("exact")), 1.2345678901234567);
        TestTrue(TEXT("Actual encoded characters are accounted"), Store.GetCharacterCount() > 0);
        TestFalse(TEXT("Foreign principal cannot read a view"), Store.ReadRows(View, Component, TEXT("foreign"), 0, 1, Page, Total, Error));
        TestEqual(TEXT("Foreign refusal clears rows"), Page.Num(), 0);
        TestEqual(TEXT("Foreign refusal clears total"), Total, 0);
        TestFalse(TEXT("Foreign principal cannot release another owner's root"), Store.Release(Root, Component, TEXT("foreign")));
        for (int32 Index = Store.GetHandleCount(); Index < FPCGDataViewStore::MaxOwnerHandles; ++Index)
            TestFalse(TEXT("Remaining owner capacity is usable"), Store.RetainRoot(Component, TEXT("owner"), 0, Error).IsEmpty());
        TestTrue(TEXT("Owner capacity is fail-closed without eviction"), Store.RetainRoot(Component, TEXT("owner"), 0, Error).IsEmpty());
        TestTrue(TEXT("Existing view survives capacity refusal"), Store.ReadRows(View, Component, TEXT("owner"), 0, 1, Page, Total, Error));
        TestTrue(TEXT("Owner releases root and derived view"), Store.Release(Root, Component, TEXT("owner")));
        TestFalse(TEXT("Root release also releases dependent projections"), Store.ReadRows(View, Component, TEXT("owner"), 0, 1, Page, Total, Error));
        TestEqual(TEXT("Released projection characters are not retained"), Store.GetCharacterCount(), static_cast<int64>(0));
        for (int32 Owner = 1; Owner <= 4 && Store.GetHandleCount() < FPCGDataViewStore::MaxHandles; ++Owner)
            for (int32 Index = 0; Index < FPCGDataViewStore::MaxOwnerHandles && Store.GetHandleCount() < FPCGDataViewStore::MaxHandles; ++Index)
                TestFalse(TEXT("Other owners can use remaining global capacity"), Store.RetainRoot(Component, FString::Printf(TEXT("owner%d"), Owner), 0, Error).IsEmpty());
        TestEqual(TEXT("Global handle boundary was actually reached"), Store.GetHandleCount(), FPCGDataViewStore::MaxHandles);
        TestTrue(TEXT("Global capacity refuses without evicting existing owners"), Store.RetainRoot(Component, TEXT("extra"), 0, Error).IsEmpty());
        const TWeakObjectPtr<UPCGComponent> Weak = Component;
        Fixture.DestroyOwned();
        CollectGarbage(RF_NoFlags);
        TestFalse(TEXT("Retained views do not keep the destroyed fixture component alive"), Weak.IsValid());
        Store.Purge();
        TestEqual(TEXT("Target loss and GC release stale entries"), Store.GetHandleCount(), 0);
        return true;
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootCharacterBudgetTest, "EngineWire.PCG.RootData.AggregateCharacterBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootCharacterBudgetTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        using namespace McpPCGHandlers;
        UPCGComponent* Component = Fixture.GetComponent();
        FPCGDataViewStore Store;
        FString Error;
        TArray<TSharedPtr<FJsonValue>> Rows;
        Rows.Init(MakeShared<FJsonValueString>(FString::ChrN(7500, TEXT('x'))), 20);
        for (int32 Owner = 0; Owner < 6; ++Owner)
        {
            const FString Principal = FString::Printf(TEXT("owner%d"), Owner);
            const FString Root = Store.RetainRoot(Component, Principal, 0, Error);
            if (!TestFalse(TEXT("Root retained within handle capacity"), Root.IsEmpty())) return true;
            if (!TestFalse(TEXT("Projection fits owner character budget"), Store.RetainRows(Root, Component, Principal, Rows, Error).IsEmpty())) return true;
            const int64 Before = Store.GetCharacterCount();
            TestTrue(TEXT("Second projection exceeds owner character budget"), Store.RetainRows(Root, Component, Principal, Rows, Error).IsEmpty());
            TestEqual(TEXT("Refused projection retains no encoded rows"), Store.GetCharacterCount(), Before);
        }
        const FString Root = Store.RetainRoot(Component, TEXT("extra"), 0, Error);
        TestTrue(TEXT("Global character budget refuses another valid projection"), Store.RetainRows(Root, Component, TEXT("extra"), Rows, Error).IsEmpty());
        TestTrue(TEXT("Character accounting stays within global bound"), Store.GetCharacterCount() <= FPCGDataViewStore::MaxCharacters);
        return true;
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootExpiryTest, "EngineWire.PCG.RootData.ExpiresWithoutRetentionLeak",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootExpiryTest::RunTest(const FString& Parameters)
{
    struct FExpiry
    {
        McpPCGHandlers::FPCGDataViewStore Store;
        FString Root, Rows;
        double Deadline = 0;
        bool bStartedBefore = false;
    };
    auto State = MakeShared<FExpiry>();
    McpPCGRootTests::Run(this, [this, State](McpPCGRootTests::FFixture& Fixture)
    {
        UPCGComponent* Component = Fixture.GetComponent();
        FString Error;
        if (State->Deadline == 0)
        {
            State->bStartedBefore = Component->OnPCGGraphStartGeneratingDelegate.IsBound();
            State->Root = State->Store.RetainRoot(Component, TEXT("owner"), 0, Error);
            TestFalse(TEXT("Expiry fixture retains a real native root"), State->Root.IsEmpty());
            const TArray<TSharedPtr<FJsonValue>> Rows{MakeShared<FJsonValueString>(TEXT("owned expiring projection"))};
            State->Rows = State->Store.RetainRows(State->Root, Component, TEXT("owner"), Rows, Error);
            TestFalse(TEXT("Expiry fixture retains encoded rows"), State->Rows.IsEmpty());
            TestTrue(TEXT("Expiry begins with retained characters"), State->Store.GetCharacterCount() > 0);
            State->Deadline = FPlatformTime::Seconds() + McpPCGHandlers::FPCGDataSnapshot::LifetimeSeconds + 1.0;
            return false;
        }
        if (FPlatformTime::Seconds() < State->Deadline) return false;
        TArray<TSharedPtr<FJsonValue>> Rows;
        int32 Total = -1;
        TestFalse(TEXT("Expired row view is refused"), State->Store.ReadRows(State->Rows, Component, TEXT("owner"), 0, 1, Rows, Total, Error));
        TestEqual(TEXT("Expired view returns no stale payload"), Rows.Num(), 0);
        TestFalse(TEXT("Expired root lookup is refused"), State->Store.ResolveRoot(State->Root, Component, TEXT("owner"), Error).IsValid());
        State->Store.Purge();
        TestEqual(TEXT("Expiry removes retained handles"), State->Store.GetHandleCount(), 0);
        TestEqual(TEXT("Expiry releases encoded characters"), State->Store.GetCharacterCount(), static_cast<int64>(0));
        TestEqual(TEXT("Expiry restores pre-existing generation-start binding state"), Component->OnPCGGraphStartGeneratingDelegate.IsBound(), State->bStartedBefore);
        return true;
    });
    return true;
}
#endif
