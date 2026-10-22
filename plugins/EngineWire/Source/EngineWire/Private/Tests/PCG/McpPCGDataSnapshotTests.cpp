#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Data/PCGPointArrayData.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSnapshotIsolationTest, "EngineWire.PCG.DataSnapshot.ImmutableProjectionAndOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSnapshotIsolationTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Data->SetNumPoints(1);
    Data->GetSeedValueRange()[0] = 41;
    auto Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("seed"), Data->GetConstSeedValueRange()[0]);
    constexpr double ExactDouble = 1.2345678901234567;
    Row->SetNumberField(TEXT("precision"), ExactDouble);
    TArray<TSharedPtr<FJsonValue>> Rows = {MakeShared<FJsonValueObject>(Row)};
    FString Error;
    auto Snapshot = CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error);
    if (!TestTrue(TEXT("A bounded native projection is retained"), Snapshot.IsValid())) return false;
    Row->SetNumberField(TEXT("seed"), 999);
    Data->GetSeedValueRange()[0] = 97;
    TArray<TSharedPtr<FJsonValue>> Page;
    TestTrue(TEXT("Owner can read its immutable projection"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    if (!TestEqual(TEXT("Exactly one projected row is returned"), Page.Num(), 1)) return false;
    TestEqual(TEXT("Native and caller-side changes cannot change captured values"), Page[0]->AsObject()->GetNumberField(TEXT("seed")), 41.0);
    TestEqual(TEXT("Retained JSON round-trip preserves native double precision"), Page[0]->AsObject()->GetNumberField(TEXT("precision")), ExactDouble);
    Page[0]->AsObject()->SetNumberField(TEXT("seed"), 123);
    TestTrue(TEXT("The next page read is independent of the earlier reply"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    TestEqual(TEXT("Returned JSON cannot mutate retained JSON"), Page[0]->AsObject()->GetNumberField(TEXT("seed")), 41.0);
    TestFalse(TEXT("A different principal cannot read retained projection data"), Snapshot->ReadPage(TEXT("other"), Component.Get(), 0, 1, Page, Error));
    TestEqual(TEXT("Failed reads clear any prior reply rows"), Page.Num(), 0);
    TestTrue(TEXT("An off-end page is valid and empty"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), 1, 1, Page, Error));
    TestEqual(TEXT("Off-end page contains no rows"), Page.Num(), 0);
    TestFalse(TEXT("Negative page offset is refused"), Snapshot->ReadPage(TEXT("owner"), Component.Get(), -1, 1, Page, Error));
    TestTrue(TEXT("The snapshot is a historical projection, not current native contents"), Snapshot->GetRowCount() == 1 && Data->GetConstSeedValueRange()[0] == 97);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSnapshotInvalidationTest, "EngineWire.PCG.DataSnapshot.NativeEventsAndTargetChangesInvalidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSnapshotInvalidationTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    const TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<TSharedPtr<FJsonValue>> Page;
    FString Error;
    const int32 StartedBefore = Component->OnPCGGraphStartGeneratingDelegate.IsBound() ? 1 : 0;
    auto Make = [&]() { return CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error); };
    {
        auto View = Make();
        if (!TestTrue(TEXT("Empty projection is a valid snapshot"), View.IsValid())) return false;
        // Test-only native delegate delivery exercises invalidation, never completion of actual scheduled work.
        Component->OnPCGGraphStartGeneratingDelegate.Broadcast(Component.Get());
        TestFalse(TEXT("Generation start invalidates even a cached-data projection"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    TestEqual(TEXT("Snapshot release removes its generation subscription"), Component->OnPCGGraphStartGeneratingDelegate.IsBound() ? 1 : 0, StartedBefore);
    {
        auto View = Make();
        if (!TestTrue(TEXT("Second projection can be created"), View.IsValid())) return false;
        Component->OnPCGGraphCleanedDelegate.Broadcast(Component.Get());
        TestFalse(TEXT("Native cleanup invalidates the projection"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    {
        auto View = Make();
        if (!TestTrue(TEXT("A completion-bound projection can be created"), View.IsValid())) return false;
        Component->OnPCGGraphGeneratedDelegate.Broadcast(Component.Get());
        TestFalse(TEXT("Generation completion invalidates old projection identity"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    {
        auto View = Make();
        if (!TestTrue(TEXT("A cancellation-bound projection can be created"), View.IsValid())) return false;
        Component->OnPCGGraphCancelledDelegate.Broadcast(Component.Get());
        TestFalse(TEXT("Generation cancellation invalidates old projection identity"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    {
        auto View = Make();
        if (!TestTrue(TEXT("Third projection can be created"), View.IsValid())) return false;
        ++Component->Seed;
        TestFalse(TEXT("A changed authored component input invalidates the projection"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
        TestEqual(TEXT("A complete changed component token is diagnosed separately"), Error,
            FString(TEXT("The projection's authored component revision changed.")));
    }
    {
        auto View = Make();
        if (!TestTrue(TEXT("Fourth projection can be created"), View.IsValid())) return false;
        TStrongObjectPtr<UPCGComponent> Other(NewObject<UPCGComponent>(World.Get()));
        TestFalse(TEXT("Another component cannot be substituted"), View->ReadPage(TEXT("owner"), Other.Get(), 0, 1, Page, Error));
        const FString MovedName = Data->GetName() + TEXT("_Moved");
        Data->Rename(*MovedName, GetTransientPackage());
        TestFalse(TEXT("A moved native dataset cannot be rebound by its old identity"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    {
        auto View = Make();
        if (!TestTrue(TEXT("A projection can weakly bind the moved data"), View.IsValid())) return false;
        Data.Reset();
        CollectGarbage(RF_NoFlags);
        TestFalse(TEXT("Snapshot retention does not keep native data alive through GC"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    }
    TestFalse(TEXT("Releasing every snapshot removes generated subscriptions"), Component->OnPCGGraphGeneratedDelegate.IsBound());
    TestFalse(TEXT("Releasing every snapshot removes cleanup subscriptions"), Component->OnPCGGraphCleanedDelegate.IsBound());
    TestFalse(TEXT("Releasing every snapshot removes cancellation subscriptions"), Component->OnPCGGraphCancelledDelegate.IsBound());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataSnapshotBudgetTest, "EngineWire.PCG.DataSnapshot.RefusesIncompleteAndOversizedProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataSnapshotBudgetTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    FString Error;
    TArray<TSharedPtr<FJsonValue>> Rows;
    Rows.Init(MakeShared<FJsonValueNumber>(1), FPCGDataSnapshot::MaxRows + 1);
    TestFalse(TEXT("An over-row projection is refused, never silently truncated"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error).IsValid());
    Rows = {MakeShared<FJsonValueString>(FString::ChrN(FPCGDataSnapshot::MaxCharacters + 1, TEXT('x')))};
    TestFalse(TEXT("An oversized string is refused before serializing it"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error).IsValid());
    Rows = {nullptr};
    TestFalse(TEXT("Invalid JSON cannot become an empty native row"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error).IsValid());
    Rows.Init(MakeShared<FJsonValueString>(FString::ChrN(8000, TEXT('x'))), 8);
    auto View = CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), Rows, Error);
    if (!TestTrue(TEXT("A bounded projection can contain several pages"), View.IsValid())) return false;
    TArray<TSharedPtr<FJsonValue>> Page;
    TestFalse(TEXT("An over-budget page is refused without partial rows"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 8, Page, Error));
    TestEqual(TEXT("Oversized page leaves no partial payload"), Page.Num(), 0);
    TestTrue(TEXT("The caller may narrow a refused page"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    Rows.Reset();
    TestFalse(TEXT("Anonymous retained projections are refused"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), FString(), Rows, Error).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPinTextProjectionTest, "EngineWire.PCG.DataSnapshot.PinTextIdentityPreservesHistoricalProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPinTextProjectionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
    UPCGPin* Pin = Node ? Node->GetInputPin(TEXT("Seed")) : nullptr;
    if (!TestNotNull(TEXT("A real authored spawner pin exists"), Pin)) return false;
    Pin->Properties.Tooltip = FText::FromString(TEXT("Projection tooltip"));
    Component->SetGraph(Graph.Get());
    TStrongObjectPtr<UPCGPointArrayData> Data(NewObject<UPCGPointArrayData>(GetTransientPackage()));
    Data->SetNumPoints(1);
    Data->GetSeedValueRange()[0] = 41;
    auto Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("seed"), Data->GetConstSeedValueRange()[0]);
    FString Error;
    auto View = CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), {MakeShared<FJsonValueObject>(Row)}, Error);
    if (!TestTrue(TEXT("An exact graph-bound point projection is retained"), View.IsValid())) return false;
    Pin->Properties.Tooltip = FText::ChangeKey(TEXT(""), TEXT("F85E815A4A4F42C195DF9BD9B5B7D93F"), Pin->Properties.Tooltip);
    TArray<TSharedPtr<FJsonValue>> Page;
    if (!TestTrue(TEXT("Text serialization identity does not invalidate the old projection"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error))) return false;
    if (!TestEqual(TEXT("The retained point row is still readable"), Page.Num(), 1)) return false;
    TestEqual(TEXT("Historical native seed is unchanged"), Page[0]->AsObject()->GetNumberField(TEXT("seed")), 41.0);
    ++Settings->Seed;
    TestFalse(TEXT("Actual authored settings changes still invalidate the same projection"), View->ReadPage(TEXT("owner"), Component.Get(), 0, 1, Page, Error));
    TestEqual(TEXT("The strict graph-revision refusal remains intact"), Error, FString(TEXT("The projection's authored graph revision changed.")));
    TestEqual(TEXT("A refused old view publishes no partial rows"), Page.Num(), 0);
    return true;
}
#endif
