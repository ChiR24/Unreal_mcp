#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataViews.h"
#include "Tests/PCG/McpPCGRootFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Data/McpPCGDataActions.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsAssetDelete.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootMembershipTest, "EngineWire.PCG.RootData.MembershipAndOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootMembershipTest::RunTest(const FString& Parameters)
{
    struct FPhase { int32 Step = 0; TSharedPtr<McpPCGHandlers::FPCGRootDataView> Root; };
    auto Phase = MakeShared<FPhase>();
    McpPCGRootTests::Run(this, [this, Phase](McpPCGRootTests::FFixture& Fixture)
    {
        using namespace McpPCGHandlers;
        UPCGComponent* Component = Fixture.GetComponent();
        FString Error;
        if (Phase->Step == 0)
        {
            const bool bDirty = Component->GetOutermost()->IsDirty();
            Phase->Root = CreatePCGRootDataView(Component, TEXT("owner"), 0, Error);
            if (!TestTrue(TEXT("A real generated root slot can be captured"), Phase->Root.IsValid())) return true;
            const UPCGData* Native = Component->GetGeneratedGraphOutput().TaggedData[0].Data.Get();
            TestEqual(TEXT("Locator resolves actual native generated output"), Phase->Root->Resolve(TEXT("owner"), Component, Error), Native);
            TestNull(TEXT("A foreign principal cannot resolve the locator"), Phase->Root->Resolve(TEXT("foreign"), Component, Error));
            TStrongObjectPtr<UPCGComponent> Other(NewObject<UPCGComponent>(Component->GetWorld()));
            TestNull(TEXT("An exact component is required"), Phase->Root->Resolve(TEXT("owner"), Other.Get(), Error));
            const auto Descriptor = Phase->Root->Describe();
            TestEqual(TEXT("Native root pin is preserved"), Descriptor->GetStringField(TEXT("pin")), Component->GetGeneratedGraphOutput().TaggedData[0].Pin.ToString());
            TestEqual(TEXT("Native point data is correctly classified"), Descriptor->GetStringField(TEXT("kind")), FString(TEXT("point")));
            TestEqual(TEXT("Locator does not infer a private operation's provenance"), Descriptor->GetStringField(TEXT("provenance")), FString(TEXT("current_component_root_generation_unknown")));
            TestEqual(TEXT("Read-only locator operations leave dirty state unchanged"), Component->GetOutermost()->IsDirty(), bDirty);
            TestFalse(TEXT("Missing root is not an empty dataset"), CreatePCGRootDataView(Component, TEXT("owner"), 1, Error).IsValid());
            Component->CleanupLocalImmediate(true);
            TestNull(TEXT("Public cleanup invalidates old membership"), Phase->Root->Resolve(TEXT("owner"), Component, Error));
            Phase->Step = 1;
            return !Fixture.Regenerate();
        }
        TestNull(TEXT("Regenerated or cache-reused output cannot retarget an old locator"), Phase->Root->Resolve(TEXT("owner"), Component, Error));
        auto Fresh = CreatePCGRootDataView(Component, TEXT("owner"), 0, Error);
        if (!TestTrue(TEXT("New output requires a fresh locator"), Fresh.IsValid())) return true;
        // Isolated delegate-delivery assertion, not a claim of actual generation completion.
        Component->OnPCGGraphStartGeneratingDelegate.Broadcast(Component);
        TestNull(TEXT("Generation start invalidates even a cached-data locator"), Fresh->Resolve(TEXT("owner"), Component, Error));
        return true;
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootTextBoundTest, "EngineWire.PCG.RootData.TextBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootTextBoundTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        FString Error;
        TestFalse(TEXT("A real generated long tag is not silently truncated"), McpPCGHandlers::CreatePCGRootDataView(Fixture.GetComponent(), TEXT("owner"), 0, Error).IsValid());
        return true;
    }, 1, FString::ChrN(McpPCGHandlers::FPCGRootDataView::MaxTagCharacters + 1, TEXT('x')));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootInventoryBoundTest, "EngineWire.PCG.RootData.InventoryBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootInventoryBoundTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        FString Error;
        TestEqual(TEXT("Native generation actually produced the over-limit number of datasets"), Fixture.GetComponent()->GetGeneratedGraphOutput().TaggedData.Num(), 257);
        TestFalse(TEXT("Oversized native root inventory is refused before locator retention"), McpPCGHandlers::CreatePCGRootDataView(Fixture.GetComponent(), TEXT("owner"), 0, Error).IsValid());
        TestFalse(TEXT("Negative root index is refused"), McpPCGHandlers::CreatePCGRootDataView(Fixture.GetComponent(), TEXT("owner"), -1, Error).IsValid());
        return true;
    }, 257);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRootSaveTextTest, "EngineWire.PCG.RootData.SafeSavePreservesRootAndTypedViews",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRootSaveTextTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        using namespace McpPCGHandlers;
        UPCGComponent* Component = Fixture.GetComponent();
        UPCGGraph* Graph = Component->GetGraph();
        const FString PackageName = TEXT("/Game/MCP_PCG_Validation/PinTextSave_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/Graph");
        if (!TestFalse(TEXT("The unique graph package has no prior disk owner"), FPackageName::DoesPackageExist(PackageName)) ||
            !TestNull(TEXT("The unique graph package is not already loaded"), FindObject<UPackage>(nullptr, *PackageName))) return true;
        UPackage* Package = CreatePackage(*PackageName);
        Graph->ClearFlags(RF_Transient);
        Graph->SetFlags(RF_Public | RF_Standalone);
        if (!TestTrue(TEXT("Only the run-owned graph moves to its new package"), Graph->Rename(TEXT("Graph"), Package, REN_DontCreateRedirectors | REN_NonTransactional))) return true;
        FPCGDataViewStore Store;
        bool bCompleted = false;
        ON_SCOPE_EXIT
        {
            Store = FPCGDataViewStore();
            if (bCompleted)
            {
                Fixture.DestroyOwned();
                TestTrue(TEXT("The exact saved fixture graph is deleted"), McpSafeOperations::McpDeleteAssetAndFile(PackageName + TEXT(".Graph")));
            }
            else AddWarning(FString::Printf(TEXT("Preserved owned pin-text save fixture for inspection: %s"), *PackageName));
        };
        UPCGSettings* Settings = nullptr;
        UPCGNode* Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
        UPCGPin* Pin = Node ? Node->GetInputPin(TEXT("Seed")) : nullptr;
        if (!TestNotNull(TEXT("The saved graph has a native spawner override pin"), Pin)) return true;
        FString RawBefore, RawAfter;
        FPCGPinProperties::StaticStruct()->ExportText(RawBefore, &Pin->Properties, nullptr, Pin, PPF_None, nullptr);
        const FString Before = GetPCGGraphRevision(Graph);
        if (!TestFalse(TEXT("The pre-save revision is complete"), Before.IsEmpty())) return true;
        TArray<FString> Handles;
        FString Error;
        auto Input = MakeShared<FJsonObject>();
        auto Inventory = QueryPCGRootData(Component, TEXT("owner"), TEXT("list_pcg_data"), Input, Store, Handles, Error);
        if (!TestTrue(TEXT("A real generated dataset is listed before saving"), Inventory.IsValid())) return true;
        const auto& Items = Inventory->GetArrayField(TEXT("items"));
        if (!TestEqual(TEXT("The actual generated root is unique"), Items.Num(), 1)) return true;
        const FString Dataset = Items[0]->AsObject()->GetStringField(TEXT("datasetId"));
        Input->SetStringField(TEXT("datasetId"), Dataset);
        Input->SetArrayField(TEXT("fields"), {MakeShared<FJsonValueString>(TEXT("seed"))});
        Input->SetNumberField(TEXT("sourceLimit"), 1);
        auto Prior = CapturePCGDataView(Component, TEXT("owner"), Input, Store, Handles, Error);
        if (!TestTrue(TEXT("A typed view exists before persistence"), Prior.IsValid())) return true;
        if (!TestTrue(TEXT("Only the selected graph is safely saved"), McpSafeOperations::McpSafeAssetSave(Graph))) return true;
        FPCGPinProperties::StaticStruct()->ExportText(RawAfter, &Pin->Properties, nullptr, Pin, PPF_None, nullptr);
        TestNotEqual(TEXT("Real persistence assigned text identity to the native tooltip"), RawAfter, RawBefore);
        TestEqual(TEXT("Persistence alone preserves authored graph revision"), GetPCGGraphRevision(Graph), Before);
        auto After = CapturePCGDataView(Component, TEXT("owner"), Input, Store, Handles, Error);
        if (!TestTrue(TEXT("The pre-save dataset handle still creates a typed view after saving"), After.IsValid())) return true;
        auto Read = MakeShared<FJsonObject>();
        for (const auto& Capture : {Prior, After})
        {
            Read->SetStringField(TEXT("viewId"), Capture->GetStringField(TEXT("viewId")));
            auto Page = QueryPCGRootData(Component, TEXT("owner"), TEXT("read_pcg_data"), Read, Store, Handles, Error);
            if (!TestTrue(TEXT("Both pre-save and post-save typed views remain readable"), Page.IsValid())) return true;
            const auto& Rows = Page->GetArrayField(TEXT("rows"));
            if (!TestEqual(TEXT("Exactly one real generated row is read"), Rows.Num(), 1)) return true;
            TestEqual(TEXT("The actual generated point seed is preserved"), Rows[0]->AsObject()->GetObjectField(TEXT("fields"))->GetNumberField(TEXT("seed")), 41.0);
        }
        ++Settings->Seed;
        TestFalse(TEXT("A genuine authored edit still invalidates the retained typed view"), QueryPCGRootData(Component, TEXT("owner"), TEXT("read_pcg_data"), Read, Store, Handles, Error).IsValid());
        TestEqual(TEXT("The real edit retains the strict graph-revision refusal"), Error, FString(TEXT("The projection's authored graph revision changed.")));
        bCompleted = true;
        return true;
    });
    return true;
}
#endif
