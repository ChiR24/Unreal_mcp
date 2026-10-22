#include "Misc/AutomationTest.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGReadBudgetTest, "EngineWire.PCG.OversizedAuthoredStateFailsClosedBeforeExport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGReadBudgetTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
    UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
    if (!TestNotNull(TEXT("Spawner"), Spawner)) return false;
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    UPCGMeshSelectorWeighted* Selector = Cast<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    if (!TestNotNull(TEXT("Owned selector"), Selector)) return false;
    const FString Before = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Small authored graph has a complete revision"), Before.IsEmpty());
    Selector->MeshEntries.SetNum(2048);
    const double Started = FPlatformTime::Seconds();
    FPCGReadBudget Budget;
    TestFalse(TEXT("Large reflected descriptor arrays exceed a shared value budget"), Budget.CheckGraph(Graph.Get()));
    TestTrue(TEXT("Exhaustion stops the field walk"), Budget.Values <= FPCGReadBudget::MaxValues + 1);
    TestTrue(TEXT("No partial revision is advertised"), GetPCGGraphRevision(Graph.Get()).IsEmpty());
    FString Error;
    TestFalse(TEXT("Omitting expectedGraphRevision cannot bypass exhaustion"),
        CheckPCGGraphRevision(Graph.Get(), MakeShared<FJsonObject>(), Error));
    TestFalse(TEXT("Budget refusal has an explanation"), Error.IsEmpty());
    AddInfo(FString::Printf(TEXT("Oversized descriptor rejection and revision check: %.3f ms"), (FPlatformTime::Seconds() - Started) * 1000.0));
    Selector->MeshEntries.Reset();
    TestEqual(TEXT("Reducing the state restores the complete original revision"), GetPCGGraphRevision(Graph.Get()), Before);
    FStrProperty* Comment = FindFProperty<FStrProperty>(Node->GetClass(), TEXT("NodeComment"));
    if (!TestNotNull(TEXT("Authored comment"), Comment)) return false;
    Comment->SetPropertyValue_InContainer(Node, FString::ChrN(FPCGReadBudget::MaxCharacters + 1, TEXT('x')));
    TestTrue(TEXT("A single oversized text value is refused before hashing"), GetPCGGraphRevision(Graph.Get()).IsEmpty());
    return true;
}
#endif
