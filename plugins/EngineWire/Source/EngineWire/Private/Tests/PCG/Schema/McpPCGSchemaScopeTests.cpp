#include "Misc/AutomationTest.h"
#include "Domains/PCG/Discovery/Schema/McpPCGSchemaScope.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Elements/PCGDataFromActor.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
UPCGNode* MakeSchemaScopeNode(FAutomationTestBase& Test, UPCGGraph* Graph, const TCHAR* ClassPath, UPCGSettings*& Settings)
{
    UClass* Class = FindObject<UClass>(nullptr, ClassPath);
    if (!Test.TestNotNull(TEXT("Fixture's native settings class is already loaded"), Class)) return nullptr;
    return Graph->AddNodeOfType(Class, Settings);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSchemaOwnedScopeTest, "EngineWire.PCG.SchemaScope.ExistingOwnedObjectsAndStructs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSchemaOwnedScopeTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = MakeSchemaScopeNode(*this, Graph.Get(), TEXT("/Script/PCG.PCGStaticMeshSpawnerSettings"), Settings);
    if (!TestNotNull(TEXT("Owned spawner node exists"), Node) || !Settings) return false;
    auto* Spawner = static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);
    UObject* Selector = Spawner->MeshSelectorParameters;
    if (!TestNotNull(TEXT("Fixture has a real initialized owned selector"), Selector)) return false;
    const FString Revision = McpPCGHandlers::GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("Read invariance requires a complete starting fingerprint"), Revision.IsEmpty())) return false;
    const bool bDirty = Graph->GetOutermost()->IsDirty();
    McpPCGHandlers::FPCGSchemaScope Scope;
    FString Error;
    TestTrue(TEXT("An empty relative scope selects existing instance settings"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT(""), Scope, Error));
    TestTrue(TEXT("Instance settings, not a CDO, are the actual container"), Scope.Container == Settings && Scope.Object == Settings && Scope.Settings == Settings);
    TestTrue(TEXT("An initialized selector's actual class is discoverable"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("MeshSelectorParameters"), Scope, Error));
    TestTrue(TEXT("Selector schema borrows the existing exact owned object"), Scope.Container == Selector && Scope.Object == Selector && Scope.Type == Selector->GetClass());
    TestEqual(TEXT("Canonical relative scope path is explicit"), Scope.Path, FString(TEXT("MeshSelectorParameters")));
    TestTrue(TEXT("Read did not replace or initialize a selector"), Spawner->MeshSelectorParameters.Get() == Selector);
    TestEqual(TEXT("Owned scope query preserves the graph revision"), McpPCGHandlers::GetPCGGraphRevision(Graph.Get()), Revision);
    TestEqual(TEXT("Owned scope query preserves package dirty state"), Graph->GetOutermost()->IsDirty(), bDirty);

    Settings = nullptr;
    UPCGNode* ActorNode = MakeSchemaScopeNode(*this, Graph.Get(), TEXT("/Script/PCG.PCGDataFromActorSettings"), Settings);
    if (!ActorNode || !Settings) return false;
    auto* ActorSettings = static_cast<UPCGDataFromActorSettings*>(Settings);
    const auto ActorFilter = ActorSettings->ActorSelector.ActorFilter;
    TestTrue(TEXT("Existing by-value selector struct is a valid scope"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), ActorNode, TEXT("ActorSelector"), Scope, Error));
    TestTrue(TEXT("Struct scope borrows exact instance struct memory"), Scope.Container == &ActorSettings->ActorSelector);
    TestTrue(TEXT("Struct scope keeps the owning settings separate from its type"), Scope.Object == Settings && Scope.Settings == Settings && Scope.Type == FPCGActorSelectorSettings::StaticStruct());
    TestEqual(TEXT("Struct discovery does not apply or normalize selector values"), ActorSettings->ActorSelector.ActorFilter, ActorFilter);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSchemaForeignScopeTest, "EngineWire.PCG.SchemaScope.RefusesForeignNullAndClassReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSchemaForeignScopeTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    TStrongObjectPtr<UPCGGraph> OtherGraph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = MakeSchemaScopeNode(*this, Graph.Get(), TEXT("/Script/PCG.PCGStaticMeshSpawnerSettings"), Settings);
    if (!Node || !Settings) return false;
    auto* Spawner = static_cast<UPCGStaticMeshSpawnerSettings*>(Settings);
    UObject* Original = Spawner->MeshSelectorParameters;
    McpPCGHandlers::FPCGSchemaScope Scope;
    FString Error;
    TestFalse(TEXT("Another graph cannot authorize this node's settings"), McpPCGHandlers::ResolvePCGSchemaScope(OtherGraph.Get(), Node, TEXT(""), Scope, Error));
    TestFalse(TEXT("Null instanced packer is unavailable, not implicitly created"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("InstanceDataPackerParameters"), Scope, Error));
    TestNull(TEXT("A query never creates an absent native packer"), Spawner->InstanceDataPackerParameters.Get());
    TestFalse(TEXT("Class reference is not a settings-owned instance scope"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("MeshSelectorType"), Scope, Error));
    TestFalse(TEXT("Array traversal is explicit unsupported, not silently a struct scope"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("MeshSelectorParameters.MeshEntries"), Scope, Error));
    TStrongObjectPtr<UPCGMeshSelectorWeighted> Foreign(NewObject<UPCGMeshSelectorWeighted>(GetTransientPackage()));
    Spawner->MeshSelectorParameters = Foreign.Get();
    TestFalse(TEXT("Shared or foreign object is not followed by a schema read"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("MeshSelectorParameters"), Scope, Error));
    TestTrue(TEXT("Failure clears every borrowed output pointer and path"), Scope.Container == nullptr && Scope.Settings == nullptr && Scope.Type == nullptr && Scope.Path.IsEmpty());
    Spawner->MeshSelectorParameters = static_cast<UPCGMeshSelectorBase*>(Original);
    TestTrue(TEXT("Only fixture setup restored its own original selector"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, TEXT("MeshSelectorParameters"), Scope, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSchemaPathScopeTest, "EngineWire.PCG.SchemaScope.RefusesMalformedAndOversizedPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSchemaPathScopeTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = MakeSchemaScopeNode(*this, Graph.Get(), TEXT("/Script/PCG.PCGStaticMeshSpawnerSettings"), Settings);
    if (!Node || !Settings) return false;
    const TArray<FString> Invalid = {TEXT(".MeshSelectorParameters"), TEXT("MeshSelectorParameters."), TEXT("MeshSelectorParameters..MeshEntries"),
        TEXT("MeshSelectorParameters.MeshEntries[0]"), TEXT("/Script/PCG.PCGMeshSelectorWeighted"), TEXT("MissingOwnedSelector"),
        TEXT("A.A.A.A.A.A.A.A.A"), FString::ChrN(513, TEXT('x'))};
    for (const FString& Path : Invalid)
    {
        McpPCGHandlers::FPCGSchemaScope Scope;
        FString Error;
        TestFalse(TEXT("Invalid path is refused"), McpPCGHandlers::ResolvePCGSchemaScope(Graph.Get(), Node, Path, Scope, Error));
        TestFalse(TEXT("A refusal explains why the scope is unavailable"), Error.IsEmpty());
        TestNull(TEXT("A refusal retains no borrowed container"), Scope.Container);
    }
    return true;
}
#endif
