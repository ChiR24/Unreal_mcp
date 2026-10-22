#include "Misc/AutomationTest.h"
#include "Domains/PCG/Data/McpPCGDataActions.h"
#include "Tests/PCG/McpPCGRootFixture.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_EXECUTION
#include "PCGData.h"
#include "Engine/World.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "PCGGraphExecutionInspection.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataInputTest, "EngineWire.PCG.DataApi.RefusesUnboundedOrCoercedInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataInputTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    auto Payload = MakeShared<FJsonObject>();
    int32 Number = 0;
    FString Error;
    Payload->SetNumberField(TEXT("limit"), 1.5);
    TestFalse(TEXT("Fractional limits are not truncated"), ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 64, Number, Error));
    Payload->SetStringField(TEXT("limit"), TEXT("2"));
    TestFalse(TEXT("String limits are not coerced"), ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 64, Number, Error));
    Payload->SetNumberField(TEXT("limit"), 2147483648.0);
    TestFalse(TEXT("Overflowing limits are rejected before the integer cast"), ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 64, Number, Error));
    Payload->SetNumberField(TEXT("limit"), 64);
    TestTrue(TEXT("Bounded integer endpoint is accepted"), ReadPCGDataInt(Payload, TEXT("limit"), 16, 1, 64, Number, Error));
    TestTrue(TEXT("Successful parsing clears previous errors"), Error.IsEmpty());
    FString Domain;
    Payload->SetBoolField(TEXT("domain"), true);
    TestFalse(TEXT("Invalid domain types do not silently use a default"), ReadPCGDataDomain(Payload, Domain, Error));
    TArray<FString> Columns;
    Payload->SetArrayField(TEXT("fields"), {MakeShared<FJsonValueString>(TEXT("seed")), MakeShared<FJsonValueNumber>(3)});
    TestFalse(TEXT("Invalid column values are not coerced"), ReadPCGDataStrings(Payload, TEXT("fields"), 64, Columns, Error));
    TestEqual(TEXT("Failed column parsing returns no partial selection"), Columns.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataFacadeTest, "EngineWire.PCG.DataApi.TypedViewFromRealGeneratedRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataFacadeTest::RunTest(const FString& Parameters)
{
    McpPCGRootTests::Run(this, [this](McpPCGRootTests::FFixture& Fixture)
    {
        using namespace McpPCGHandlers;
        UPCGComponent* Component = Fixture.GetComponent();
        const FString GraphBefore = GetPCGGraphRevision(Component->GetGraph());
        const FString InstanceBefore = GetPCGInstanceRevision(Component);
        if (!TestFalse(TEXT("Read invariance starts with an actual graph revision"), GraphBefore.IsEmpty()) ||
            !TestFalse(TEXT("Read invariance starts with an actual instance revision"), InstanceBefore.IsEmpty())) return true;
        const bool bDirtyBefore = Component->GetOutermost()->IsDirty();
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
        const bool bInspectionBefore = Component->GetExecutionState().GetInspection().IsInspecting();
#endif
        FPCGDataViewStore Store;
        TArray<FString> Handles;
        FString Error;
        auto Input = MakeShared<FJsonObject>();
        auto Inventory = QueryPCGRootData(Component, TEXT("owner"), TEXT("list_pcg_data"), Input, Store, Handles, Error);
        if (!TestTrue(TEXT("Root discovery returns the actual native dataset"), Inventory.IsValid())) return true;
        const auto& Items = Inventory->GetArrayField(TEXT("items"));
        if (!TestEqual(TEXT("Exactly one native root is inventoried"), Items.Num(), 1)) return true;
        const FString Dataset = Items[0]->AsObject()->GetStringField(TEXT("datasetId"));
        Input->SetStringField(TEXT("datasetId"), Dataset);
        auto Schema = QueryPCGRootData(Component, TEXT("owner"), TEXT("get_pcg_data_schema"), Input, Store, Handles, Error);
        TestTrue(TEXT("Discovered root has a real const metadata schema"), Schema.IsValid());
        Input->SetArrayField(TEXT("fields"), {MakeShared<FJsonValueString>(TEXT("seed")), MakeShared<FJsonValueString>(TEXT("transform"))});
        auto Capture = CapturePCGDataView(Component, TEXT("owner"), Input, Store, Handles, Error);
        if (!TestTrue(TEXT("Typed range is captured from this exact real root"), Capture.IsValid())) return true;
        const FString View = Capture->GetStringField(TEXT("viewId"));
        Capture->GetObjectField(TEXT("projection"))->SetStringField(TEXT("domain"), TEXT("tampered"));
        auto ReadInput = MakeShared<FJsonObject>();
        ReadInput->SetStringField(TEXT("viewId"), View);
        auto Page = QueryPCGRootData(Component, TEXT("owner"), TEXT("read_pcg_data"), ReadInput, Store, Handles, Error);
        if (!TestTrue(TEXT("Captured typed view is readable"), Page.IsValid())) return true;
        const auto& Rows = Page->GetArrayField(TEXT("rows"));
        if (!TestEqual(TEXT("Native source row was captured"), Rows.Num(), 1)) return true;
        TestEqual(TEXT("Native point seed survives the entire internal data workflow"), Rows[0]->AsObject()->GetObjectField(TEXT("fields"))->GetNumberField(TEXT("seed")), 41.0);
        TestEqual(TEXT("Reply mutation cannot change retained projection metadata"), Page->GetObjectField(TEXT("projection"))->GetStringField(TEXT("domain")), FString(TEXT("Elements")));
        TestEqual(TEXT("Rotation encoding is explicitly declared"), Page->GetObjectField(TEXT("projection"))->GetStringField(TEXT("pointRotationEncoding")), FString(TEXT("quaternion_xyzw")));
        TestFalse(TEXT("Foreign owner cannot read the same view"), QueryPCGRootData(Component, TEXT("foreign"), TEXT("read_pcg_data"), ReadInput, Store, Handles, Error).IsValid());
        auto Release = MakeShared<FJsonObject>();
        Release->SetStringField(TEXT("handleId"), Dataset);
        TestTrue(TEXT("Owner explicitly releases root plus dependent projections"), QueryPCGRootData(Component, TEXT("owner"), TEXT("release_pcg_data_view"), Release, Store, Handles, Error).IsValid());
        TestEqual(TEXT("Release leaves no retained workflow state"), Store.GetHandleCount(), 0);
        TestFalse(TEXT("Released row view cannot silently bind a newer source"), QueryPCGRootData(Component, TEXT("owner"), TEXT("read_pcg_data"), ReadInput, Store, Handles, Error).IsValid());
        TestEqual(TEXT("Data workflow does not change authored graph revision"), GetPCGGraphRevision(Component->GetGraph()), GraphBefore);
        TestEqual(TEXT("Data workflow does not change authored component inputs"), GetPCGInstanceRevision(Component), InstanceBefore);
        TestEqual(TEXT("Data workflow does not dirty the component package"), Component->GetOutermost()->IsDirty(), bDirtyBefore);
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7) || PCG_PROFILING_ENABLED
        TestEqual(TEXT("Data workflow preserves native execution-inspection state"), Component->GetExecutionState().GetInspection().IsInspecting(), bInspectionBefore);
#endif
        return true;
    });
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDataModeTest, "EngineWire.PCG.DataApi.RefusesRuntimeAndPartitionedProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDataModeTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UWorld> World(NewObject<UWorld>(GetTransientPackage()));
    TStrongObjectPtr<UPCGComponent> Component(NewObject<UPCGComponent>(World.Get()));
    TStrongObjectPtr<UPCGData> Data(NewObject<UPCGData>(GetTransientPackage()));
    FString Error;
    Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateAtRuntime;
    TestFalse(TEXT("Runtime-managed storage is not advertised as an editor-local safe view"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), {}, Error).IsValid());
    Component->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    Component->bIsComponentPartitioned = true;
    TestFalse(TEXT("Partitioned storage requires its independent lifetime contract"), CreatePCGDataSnapshot(Component.Get(), Data.Get(), TEXT("owner"), {}, Error).IsValid());
    return true;
}
#endif
