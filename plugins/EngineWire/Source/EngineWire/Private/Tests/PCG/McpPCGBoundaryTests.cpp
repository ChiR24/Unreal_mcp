#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "EngineWireSubsystem.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectHash.h"
#include "Foundation/Reflection/McpPropertyReflection.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG
#include "PCGGraph.h"
#include "PCGSubgraph.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBoundaryExternalReferenceTest, "EngineWire.PCG.BoundaryRejectsSharedSubgraphSettingsWrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBoundaryExternalReferenceTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> SourceGraph(NewObject<UPCGGraph>(GetTransientPackage()));
    TStrongObjectPtr<UPCGGraph> ReferencedGraph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = SourceGraph->AddNodeOfType(UPCGSubgraphSettings::StaticClass(), Settings);
    UPCGSubgraphSettings* Subgraph = Cast<UPCGSubgraphSettings>(Settings);
    if (!TestNotNull(TEXT("Source subgraph settings"), Subgraph) || !TestNotNull(TEXT("Subgraph instance"), Subgraph->SubgraphInstance.Get())) return false;
    Subgraph->SubgraphInstance->SetGraph(ReferencedGraph.Get());

    FBoolProperty* ExposeToLibrary = FindFProperty<FBoolProperty>(UPCGGraphInterface::StaticClass(), TEXT("bExposeToLibrary"));
    if (!TestNotNull(TEXT("Native editable graph property"), ExposeToLibrary)) return false;
    ExposeToLibrary->SetPropertyValue_InContainer(ReferencedGraph.Get(), true);
    const FString SourceRevision = GetPCGGraphRevision(SourceGraph.Get());
    const FString ReferencedRevision = GetPCGGraphRevision(ReferencedGraph.Get());
    auto SettingsValues = MakeShared<FJsonObject>();
    SettingsValues->SetBoolField(TEXT("SubgraphInstance.Graph.bExposeToLibrary"), false);
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), SettingsValues);
    FString Error;

    TestFalse(TEXT("A nested write through a referenced graph is refused"),
        ValidatePCGNodeSettings(SourceGraph.Get(), Node, TEXT("set_pcg_node_settings"), Payload, Error));
    TestTrue(TEXT("Refusal explains the shared-object boundary"), Error.Contains(TEXT("shared assets")));
    TestTrue(TEXT("Referenced graph property remains unchanged"), ExposeToLibrary->GetPropertyValue_InContainer(ReferencedGraph.Get()));
    TestEqual(TEXT("Source graph revision unchanged"), GetPCGGraphRevision(SourceGraph.Get()), SourceRevision);
    TestEqual(TEXT("Referenced graph revision unchanged"), GetPCGGraphRevision(ReferencedGraph.Get()), ReferencedRevision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBoundaryUndoRedoTest, "EngineWire.PCG.BoundaryEditorStateOnlyUsesNativeUndoRedo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBoundaryUndoRedoTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    if (!TestNotNull(TEXT("Editor transaction buffer"), GEditor ? GEditor->Trans.Get() : nullptr)) return false;
    if (!TestFalse(TEXT("No caller transaction is active"), GEditor->IsTransactionActive())) return false;

    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* SettingsClass = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings class"), SettingsClass)) return false;
    UPCGSettings* FirstSettings = nullptr;
    UPCGSettings* SecondSettings = nullptr;
    UPCGNode* First = Graph->AddNodeOfType(SettingsClass, FirstSettings);
    UPCGNode* Second = Graph->AddNodeOfType(SettingsClass, SecondSettings);
    if (!TestNotNull(TEXT("First graph node"), First) || !TestNotNull(TEXT("Second graph node"), Second) ||
        !TestNotNull(TEXT("First node model"), FirstSettings) || !TestNotNull(TEXT("Second node model"), SecondSettings)) return false;
    if (!TestFalse(TEXT("Initial graph has no edge"), HasPCGEdge(First->GetOutputPins()[0], Second->GetInputPins()[0]))) return false;

    TArray<UObject*> CapturedObjects;
    {
        FPCGEditState State(Graph.Get());
        CapturedObjects = State.GetObjects();
    }
    for (UObject* Object : CapturedObjects) if (Object) Object->SetFlags(RF_Transactional);
    const FString RevisionBefore = GetPCGGraphRevision(Graph.Get());
    FIntProperty* Seed = FindFProperty<FIntProperty>(FirstSettings->GetClass(), TEXT("Seed"));
    if (!TestNotNull(TEXT("First settings seed"), Seed)) return false;
    const int32 SeedBefore = Seed->GetPropertyValue_InContainer(FirstSettings);
    UPCGPin* OutputPin = First->GetOutputPins()[0];
    UPCGPin* InputPin = Second->GetInputPins()[0];
    bool bRecorded = false;
    bool bAddedEdge = false;
    FString RevisionAfter;
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("PCG Boundary Undo Redo")),
            EMcpMutationDurability::EditorStateOnly, CapturedObjects);
        bRecorded = Transaction.IsRecordingUndo();
        if (!bRecorded)
        {
            TestEqual(TEXT("Native helper refusal is reported truthfully"), Transaction.GetState(), EMcpTransactionState::RefusedNonTransactionalObject);
            TestEqual(TEXT("Refused transaction has a stable reason"), Transaction.GetRefusalCode(), FString(TEXT("UNDO_UNAVAILABLE_NON_TRANSACTIONAL_OBJECT")));
        }
        else
        {
            Graph->AddLabeledEdge(First, OutputPin->Properties.Label, Second, InputPin->Properties.Label);
            bAddedEdge = HasPCGEdge(OutputPin, InputPin);
            if (bAddedEdge) Seed->SetPropertyValue_InContainer(FirstSettings, SeedBefore + 1);
            RevisionAfter = GetPCGGraphRevision(Graph.Get());
        }
    }
    if (!bRecorded)
    {
        AddWarning(TEXT("The native helper correctly refused recording; this run does not establish Undo/Redo availability."));
        TestEqual(TEXT("Refusal occurs before mutation"), GetPCGGraphRevision(Graph.Get()), RevisionBefore);
        TestFalse(TEXT("Refused scope never leaves a transaction active"), GEditor->IsTransactionActive());
        return true;
    }
    if (!TestTrue(TEXT("Connection mutation committed"), bAddedEdge) || !TestNotEqual(TEXT("Recorded mutation changes revision"), RevisionAfter, RevisionBefore)) return false;
    TestFalse(TEXT("Owned transaction is closed before undo"), GEditor->IsTransactionActive());
    if (!TestTrue(TEXT("The just-created native recording is undoable"), GEditor->Trans->CanUndo())) return false;
    TestTrue(TEXT("Undo only the synchronous recording created above"), GEditor->UndoTransaction());
    TestEqual(TEXT("Undo restores exact authored revision"), GetPCGGraphRevision(Graph.Get()), RevisionBefore);
    TestTrue(TEXT("Graph node identities survive undo"), Graph->Contains(First) && Graph->Contains(Second));
    TestTrue(TEXT("Settings model identities survive undo"), First->GetSettings() == FirstSettings && Second->GetSettings() == SecondSettings);
    TestFalse(TEXT("Undo removes the added edge between the original pins"), HasPCGEdge(OutputPin, InputPin));
    TestEqual(TEXT("Undo restores settings value"), Seed->GetPropertyValue_InContainer(FirstSettings), SeedBefore);

    TestTrue(TEXT("Redo only the recording just undone"), GEditor->RedoTransaction());
    TestEqual(TEXT("Redo restores exact edited revision"), GetPCGGraphRevision(Graph.Get()), RevisionAfter);
    TestTrue(TEXT("Graph node identities survive redo"), Graph->Contains(First) && Graph->Contains(Second));
    TestTrue(TEXT("Settings model identities survive redo"), First->GetSettings() == FirstSettings && Second->GetSettings() == SecondSettings);
    TestTrue(TEXT("Redo restores the edge between the original pins"), HasPCGEdge(OutputPin, InputPin));
    TestEqual(TEXT("Redo restores edited settings value"), Seed->GetPropertyValue_InContainer(FirstSettings), SeedBefore + 1);
    AddInfo(TEXT("An actual owned native Undo/Redo recording was exercised; production recovery did not use Undo."));
    return true;
}

#endif
