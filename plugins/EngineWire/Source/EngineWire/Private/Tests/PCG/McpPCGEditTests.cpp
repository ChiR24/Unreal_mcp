#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Editing/McpPCGConnect.h"
#include "UObject/GarbageCollection.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGPreflightTest, "EngineWire.PCG.PreflightLeavesGraphUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGPreflightTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings class"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
    if (!TestNotNull(TEXT("Transform settings"), Settings)) return false;
    FIntProperty* Seed = FindFProperty<FIntProperty>(Settings->GetClass(), TEXT("Seed"));
    if (!TestNotNull(TEXT("Seed property"), Seed)) return false;
    Seed->SetPropertyValue_InContainer(Settings, 123);
    const FString Revision = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Revision is nonempty"), Revision.IsEmpty());
    auto Values = MakeShared<FJsonObject>();
    Values->SetNumberField(TEXT("Seed"), 999);
    Values->SetNumberField(TEXT("MCP_UnknownProperty"), 1);
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Values);
    FString Error;
    TestFalse(TEXT("Mixed valid and invalid settings refused"), ValidatePCGNodeSettings(Graph.Get(), Node, TEXT("set_pcg_node_settings"), Payload, Error));
    TestEqual(TEXT("Seed unchanged after refusal"), Seed->GetPropertyValue_InContainer(Settings), 123);
    TestEqual(TEXT("Full authored graph unchanged"), GetPCGGraphRevision(Graph.Get()), Revision);
    UClass* SpawnerClass = ResolvePCGSettingsClass(TEXT("PCGStaticMeshSpawnerSettings"));
    if (!TestNotNull(TEXT("Spawner settings class"), SpawnerClass)) return false;
    auto InvalidMesh = MakeShared<FJsonObject>();
    InvalidMesh->SetStringField(TEXT("meshPath"), TEXT("/Game/PCGValidation_MissingMesh"));
    TestNull(TEXT("Invalid mesh refused before node creation"), PreparePCGSettings(SpawnerClass->GetDefaultObject<UPCGSettings>(), TEXT("add_static_mesh_spawner"), InvalidMesh, Error));
    TestEqual(TEXT("Graph remains unchanged after invalid mesh"), GetPCGGraphRevision(Graph.Get()), Revision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRecoveryTest, "EngineWire.PCG.ExplicitRecoveryPreservesIdentityAfterGC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings class"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(Class, Settings);
    FIntProperty* Seed = Settings ? FindFProperty<FIntProperty>(Settings->GetClass(), TEXT("Seed")) : nullptr;
    if (!TestNotNull(TEXT("Seed property"), Seed)) return false;
    Seed->SetPropertyValue_InContainer(Settings, 123);
    const FString Revision = GetPCGGraphRevision(Graph.Get());
    {
        FPCGEditState State(Graph.Get());
        Seed->SetPropertyValue_InContainer(Settings, 999);
        UPCGSettings* AddedSettings = nullptr;
        UPCGNode* Added = Graph->AddNodeOfType(Class, AddedSettings);
        CollectGarbage(RF_NoFlags);
        TestTrue(TEXT("Explicit recovery verified"), State.Restore());
        TestEqual(TEXT("Original node identity retained"), FindPCGNode(Graph.Get(), Node->GetName()), Node);
        TestEqual(TEXT("Original settings identity retained"), Node->GetSettings(), Settings);
        TestEqual(TEXT("Original seed restored"), Seed->GetPropertyValue_InContainer(Settings), 123);
        TestFalse(TEXT("New node is not left in the graph"), Graph->Contains(Added));
    }
    TestEqual(TEXT("Original graph revision restored"), GetPCGGraphRevision(Graph.Get()), Revision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGConnectionTest, "EngineWire.PCG.IncompatibleConnectionIsPureValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGConnectionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* GridClass = ResolvePCGSettingsClass(TEXT("PCGCreatePointsGridSettings"));
    UClass* SplineClass = ResolvePCGSettingsClass(TEXT("PCGSplineSamplerSettings"));
    if (!TestNotNull(TEXT("Grid class"), GridClass) || !TestNotNull(TEXT("Spline sampler class"), SplineClass)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Grid = Graph->AddNodeOfType(GridClass, Settings);
    UPCGNode* Spline = Graph->AddNodeOfType(SplineClass, Settings);
    if (!Grid || !Spline || Grid->GetOutputPins().IsEmpty() || Spline->GetInputPins().IsEmpty()) return false;
    const FString Revision = GetPCGGraphRevision(Graph.Get());
    FString Error, Code;
    TestFalse(TEXT("Point to polyline refused"), ValidatePCGConnection(Grid->GetOutputPins()[0], Spline->GetInputPins()[0], Error, Code));
    TestFalse(TEXT("Refusal contains an explanation"), Error.IsEmpty());
    TestEqual(TEXT("Validation did not mutate the graph"), GetPCGGraphRevision(Graph.Get()), Revision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGRevisionTest, "EngineWire.PCG.AuthoredRevisionIsStableAndPrecise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGRevisionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings class"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    Graph->AddNodeOfType(Class, Settings);
    if (!TestNotNull(TEXT("Transform settings"), Settings)) return false;
    const FString Original = GetPCGGraphRevision(Graph.Get());
    for (int32 Index = 0; Index < 8; ++Index)
    {
        TestEqual(TEXT("Unchanged graph retains revision despite native FText history"), GetPCGGraphRevision(Graph.Get()), Original);
    }
    FStructProperty* Offset = FindFProperty<FStructProperty>(Settings->GetClass(), TEXT("OffsetMin"));
    if (!TestNotNull(TEXT("Offset vector property"), Offset)) return false;
    FVector* Value = Offset->ContainerPtrToValuePtr<FVector>(Settings);
    Value->X = 1.000000001;
    const FString Before = GetPCGGraphRevision(Graph.Get());
    Value->X = 1.000000002;
    TestNotEqual(TEXT("Sub-display-precision changes invalidate the authored revision"), GetPCGGraphRevision(Graph.Get()), Before);
    return true;
}
#endif
