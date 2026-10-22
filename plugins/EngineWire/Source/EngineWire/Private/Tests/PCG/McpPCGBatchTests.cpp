#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "UObject/GarbageCollection.h"
#if MCP_HAS_PCG_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "EdGraphNode_Comment.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBatchPreflightTest, "EngineWire.PCG.BatchTemporaryIdsAndPermissionBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBatchPreflightTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    FPCGMutationContext Context;
    FString Error, Code;
    TSharedPtr<FJsonObject> Result;
    auto Add = MakeShared<FJsonObject>();
    Add->SetStringField(TEXT("op"), TEXT("add_node"));
    Add->SetStringField(TEXT("settingsClass"), TEXT("PCGTransformPointsSettings"));
    Add->SetStringField(TEXT("tempId"), TEXT("first"));
    TestTrue(TEXT("Temporary node creation"), ApplyPCGMutation(Graph.Get(), Add, Context, Result, Error, Code));
    TestTrue(TEXT("Temporary ID resolved to a stable node ID"), Context.TemporaryNodes.Contains(TEXT("first")));
    UPCGNode* Created = FindPCGNode(Graph.Get(), Context.TemporaryNodes.FindRef(TEXT("first")));
    if (!TestNotNull(TEXT("Committed batch node"), Created) || !TestNotNull(TEXT("Committed batch settings"), Created->GetSettings())) return false;
    TestFalse(TEXT("Committed node does not inherit preflight transient flags"), Created->HasAnyFlags(RF_Transient));
    TestFalse(TEXT("Committed settings must be serialized when the graph is saved"), Created->GetSettings()->HasAnyFlags(RF_Transient));
    TestEqual(TEXT("Committed settings are privately owned by their node"), Created->GetSettings()->GetOuter(), static_cast<UObject*>(Created));
    auto Move = MakeShared<FJsonObject>();
    Move->SetStringField(TEXT("op"), TEXT("move_node"));
    Move->SetStringField(TEXT("nodeId"), TEXT("first"));
    Move->SetNumberField(TEXT("x"), 700);
    Move->SetNumberField(TEXT("y"), 200);
    TestTrue(TEXT("Later step resolves earlier temporary node"), ApplyPCGMutation(Graph.Get(), Move, Context, Result, Error, Code));
    const FString Revision = GetPCGGraphRevision(Graph.Get());
    auto Remove = MakeShared<FJsonObject>();
    Remove->SetStringField(TEXT("op"), TEXT("remove_node"));
    Remove->SetStringField(TEXT("nodeId"), TEXT("first"));
    TestFalse(TEXT("Write context refuses destructive step"), ApplyPCGMutation(Graph.Get(), Remove, Context, Result, Error, Code));
    TestEqual(TEXT("Exact destructive refusal"), Code, FString(TEXT("DESTRUCTIVE_OPERATION_REFUSED")));
    TestEqual(TEXT("No mutation on refusal"), GetPCGGraphRevision(Graph.Get()), Revision);
    Context.bDestructive = true;
    TestTrue(TEXT("Explicit destructive context may remove the node"), ApplyPCGMutation(Graph.Get(), Remove, Context, Result, Error, Code));
    TestEqual(TEXT("Removed only the selected node"), Graph->GetNodes().Num(), 0);
    Remove->SetStringField(TEXT("nodeId"), TEXT("input"));
    TestFalse(TEXT("Input cannot be deleted even in a destructive context"), ApplyPCGMutation(Graph.Get(), Remove, Context, Result, Error, Code));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBatchRecoveryTest, "EngineWire.PCG.CommitFaultRestoresDeletedNodesEdgesAndOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBatchRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* First = Graph->AddNodeOfType(Class, Settings);
    UPCGNode* Second = Graph->AddNodeOfType(Class, Settings);
    Graph->AddLabeledEdge(First, First->GetOutputPins()[0]->Properties.Label, Second, Second->GetInputPins()[0]->Properties.Label);
    const FString Revision = GetPCGGraphRevision(Graph.Get());
    const TArray<UPCGNode*> Order = Graph->GetNodes();
    {
        FPCGEditState State(Graph.Get());
        Graph->RemoveNode(First);
        Graph->AddNodeOfType(Class, Settings);
        CollectGarbage(RF_NoFlags);
        TestTrue(TEXT("Explicit recovery after a simulated commit fault"), State.Restore());
        TestTrue(TEXT("Original node order and identities restored"), Graph->GetNodes() == Order);
        TestTrue(TEXT("Original edge restored"), HasPCGEdge(First->GetOutputPins()[0], Second->GetInputPins()[0]));
    }
    TestEqual(TEXT("Complete authored state restored"), GetPCGGraphRevision(Graph.Get()), Revision);
    return true;
}

#if MCP_HAS_PCG_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGEditorSyncTest, "EngineWire.PCG.EditorSyncPreservesCommentsAndModelIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGEditorSyncTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UAssetEditorSubsystem* Editors = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Asset editor subsystem"), Editors)) return false;
    Editors->OpenEditorForAsset(Graph.Get());
    bool bAmbiguous = false;
    UEdGraph* Editor = FindExistingPCGEditorGraph(Graph.Get(), bAmbiguous);
    if (!TestNotNull(TEXT("Native graph editor initialized"), Editor)) return false;
    UEdGraphNode_Comment* Comment = NewObject<UEdGraphNode_Comment>(Editor, NAME_None, RF_Transactional);
    Comment->NodeComment = TEXT("Owned comment must survive synchronization");
    Comment->NodePosX = -300;
    Comment->NodeWidth = 800;
    Editor->AddNode(Comment, false, false);
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    UPCGSettings* Settings = nullptr;
    UPCGNode* Model = Class ? Graph->AddNodeOfType(Class, Settings) : nullptr;
    if (!TestNotNull(TEXT("Backend model node"), Model)) return false;
    Model->SetNodePosition(900, 400);
    const FString Before = GetPCGGraphRevision(Graph.Get());
    TSharedPtr<FJsonObject> State;
    FString Error;
    TestTrue(TEXT("Native public API synchronization"), SyncPCGEditorGraph(Graph.Get(), State, Error));
    TestEqual(TEXT("Sync does not replace or change authored models"), GetPCGGraphRevision(Graph.Get()), Before);
    TestTrue(TEXT("Model identity retained"), Graph->Contains(Model) && Model->GetSettings() == Settings);
    TestTrue(TEXT("Comment object retained"), Editor->Nodes.Contains(Comment));
    TestEqual(TEXT("Comment text retained"), Comment->NodeComment, FString(TEXT("Owned comment must survive synchronization")));
    TestEqual(TEXT("Comment layout retained"), Comment->NodeWidth, 800);
    TestTrue(TEXT("Complete model/editor mapping is consistent"), CheckPCGEditorGraph(Graph.Get())->GetBoolField(TEXT("consistent")));
    Editors->CloseAllEditorsForAsset(Graph.Get());
    Editors->OpenEditorForAsset(Graph.Get());
    TestTrue(TEXT("Mapping remains consistent after close/reopen"), CheckPCGEditorGraph(Graph.Get())->GetBoolField(TEXT("consistent")));
    Editors->CloseAllEditorsForAsset(Graph.Get());
    return true;
}
#endif
#endif
