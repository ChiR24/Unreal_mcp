#include "Misc/AutomationTest.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Curves/CurveFloat.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGTransientRevisionTest, "EngineWire.PCG.TransientChildrenDoNotChangeAuthoredRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGTransientRevisionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), Settings);
    UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
    if (!TestNotNull(TEXT("Spawner"), Spawner)) return false;
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    UPCGMeshSelectorWeighted* Selector = Cast<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    if (!TestNotNull(TEXT("Owned selector"), Selector)) return false;
    Selector->MeshEntries.AddDefaulted_GetRef().Weight = 1;
    const FString Original = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Complete initial revision"), Original.IsEmpty());
    const auto Before = ReadPCGSettings(Settings);
    UObject* RuntimeCache = NewObject<UCurveFloat>(Settings, NAME_None, RF_Transient);
    TStrongObjectPtr<UObject> KeepCache(RuntimeCache);
    NewObject<UCurveFloat>(RuntimeCache, NAME_None, RF_NoFlags);
    TestEqual(TEXT("Execution-only descendants do not alter revision"), GetPCGGraphRevision(Graph.Get()), Original);
    TestEqual(TEXT("Execution-only descendants are not authored settings"),
        ReadPCGSettings(Settings)->GetObjectField(TEXT("$ownedObjects"))->Values.Num(),
        Before->GetObjectField(TEXT("$ownedObjects"))->Values.Num());
    Selector->MeshEntries[0].Weight = 2;
    TestNotEqual(TEXT("Real owned selector edits remain visible"), GetPCGGraphRevision(Graph.Get()), Original);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSubgraphRevisionTest, "EngineWire.PCG.SubgraphEditsInvalidateExecutionInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSubgraphRevisionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    TStrongObjectPtr<UPCGGraph> Subgraph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    Graph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), Settings);
    UPCGSubgraphSettings* SubgraphSettings = Cast<UPCGSubgraphSettings>(Settings);
    if (!TestNotNull(TEXT("Subgraph settings"), SubgraphSettings)) return false;
    SubgraphSettings->SetSubgraph(Subgraph.Get());
    UPCGSettings* Transform = nullptr;
    Subgraph->AddNodeOfType(ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings")), Transform);
    if (!TestNotNull(TEXT("Subgraph transform"), Transform)) return false;
    FIntProperty* Seed = FindFProperty<FIntProperty>(Transform->GetClass(), TEXT("Seed"));
    if (!TestNotNull(TEXT("Transform seed"), Seed)) return false;
    const FString Original = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Complete graph revision"), Original.IsEmpty());
    TestEqual(TEXT("Repeated dependency capture is stable"), GetPCGGraphRevision(Graph.Get()), Original);
    Seed->SetPropertyValue_InContainer(Transform, Seed->GetPropertyValue_InContainer(Transform) + 1);
    TestNotEqual(TEXT("Referenced graph input edits cannot be called unchanged"), GetPCGGraphRevision(Graph.Get()), Original);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGDetachedRevisionTest, "EngineWire.PCG.DetachedModelObjectsDoNotChangeAuthoredRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGDetachedRevisionTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings")), Settings);
    if (!TestNotNull(TEXT("Registered model node"), Node) || Node->GetInputPins().IsEmpty()) return false;
    const FString Before = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Initial authored capture is complete"), Before.IsEmpty());
    UPCGPin* DetachedPin = NewObject<UPCGPin>(Node, NAME_None, RF_Transactional);
    DetachedPin->Node = Node;
    NewObject<UCurveFloat>(DetachedPin);
    UPCGNode* DetachedNode = NewObject<UPCGNode>(Graph.Get(), NAME_None, RF_Transactional);
    NewObject<UCurveFloat>(DetachedNode);
    TestFalse(TEXT("Detached pin is not an active model member"), IsPCGAuthoredObject(DetachedPin, Graph.Get()));
    TestFalse(TEXT("Detached node is not an active model member"), IsPCGAuthoredObject(DetachedNode, Graph.Get()));
    TestEqual(TEXT("Detached dynamic pin and node descendants do not alter revision"), GetPCGGraphRevision(Graph.Get()), Before);
    UPCGPin* Pin = Node->GetInputPins()[0];
    const bool bMultiple = Pin->Properties.bAllowMultipleData;
    Pin->Properties.bAllowMultipleData = !bMultiple;
    TestNotEqual(TEXT("Actual authored pin restrictions remain fingerprinted"), GetPCGGraphRevision(Graph.Get()), Before);
    Pin->Properties.bAllowMultipleData = bMultiple;
    TestEqual(TEXT("Restoring authored pin state restores the token"), GetPCGGraphRevision(Graph.Get()), Before);
    return true;
}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGParameterPresentationTest, "EngineWire.PCG.ParameterPresentationDoesNotChangeAuthoredRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGParameterPresentationTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    FPropertyBagPropertyDesc Desc(FName(TEXT("HierarchyParameter")), EPropertyBagPropertyType::Double);
    Desc.ID = FGuid::NewGuid();
    if (!TestTrue(TEXT("Native parameter schema added"), Graph->AddUserParameters({Desc}) == EPropertyBagAlterationResult::Success)) return false;
    const FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(Graph->GetClass(), TEXT("UserParameterHierarchyRoot"));
    if (!TestNotNull(TEXT("Native editor-only parameter hierarchy property"), RootProperty)) return false;
    TestTrue(TEXT("Presentation hierarchy is explicitly editor-only"), RootProperty->HasAnyPropertyFlags(CPF_EditorOnly));
    UObject* Hierarchy = RootProperty->GetObjectPropertyValue_InContainer(Graph.Get());
    UClass* PropertyClass = FindObject<UClass>(nullptr, TEXT("/Script/StructUtilsEditor.PropertyBagHierarchyProperty"));
    if (!Hierarchy || !PropertyClass)
    {
        AddWarning(TEXT("Native presentation classes were not loaded; UI-only hierarchy coverage is not established in this module combination."));
        return true;
    }
    const FString Before = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Initial parameter capture is complete"), Before.IsEmpty());
    UObject* Presentation = NewObject<UObject>(Hierarchy, PropertyClass, NAME_None, RF_Transactional);
    TestFalse(TEXT("Lazily allocated parameter presentation is not execution input"), IsPCGAuthoredObject(Presentation, Graph.Get()));
    TestEqual(TEXT("Opening the parameter presentation does not change revision"), GetPCGGraphRevision(Graph.Get()), Before);
    Graph->SetGraphParameter(TEXT("HierarchyParameter"), 41.0);
    TestNotEqual(TEXT("Actual native parameter values remain part of authored revision"), GetPCGGraphRevision(Graph.Get()), Before);
    return true;
}
#endif
#endif
