#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Domains/PCG/Editing/McpPCGConnect.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
namespace
{
TSharedPtr<FJsonObject> PointBoundaryPayload(const TCHAR* Label)
{
    auto Id = MakeShared<FJsonObject>();
    Id->SetStringField(TEXT("Struct"), TEXT("/Script/PCG.PCGDataTypeInfoPoint"));
    auto Types = MakeShared<FJsonObject>();
    Types->SetArrayField(TEXT("Ids"), {MakeShared<FJsonValueObject>(Id)});
    Types->SetNumberField(TEXT("CustomSubtype"), -1);
    auto Pin = MakeShared<FJsonObject>();
    Pin->SetStringField(TEXT("Label"), Label);
    Pin->SetObjectField(TEXT("AllowedTypes"), Types);
    auto Settings = MakeShared<FJsonObject>();
    Settings->SetArrayField(TEXT("Pins"), {MakeShared<FJsonValueObject>(Pin)});
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Settings);
    return Payload;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGBoundarySettingsTest, "EngineWire.PCG.BoundarySettingsUseOwnedGraphAndReadableIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGBoundarySettingsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    for (UPCGNode* Boundary : {Graph->GetInputNode(), Graph->GetOutputNode()})
    {
        if (!TestNotNull(TEXT("Native default boundary node"), Boundary)) return false;
        TestEqual(TEXT("Read node ID resolves back to the same boundary"), FindPCGNode(Graph.Get(), Boundary->GetName()), Boundary);
        TestEqual(TEXT("Exact boundary path resolves without selecting another node"), FindPCGNode(Graph.Get(), Boundary->GetPathName()), Boundary);
        TestEqual(TEXT("Native boundary settings are owned by this graph"), Boundary->GetSettings()->GetOuter(), static_cast<UObject*>(Graph.Get()));
        TestTrue(TEXT("Only these owned settings are editable"), HasOwnedPCGNodeSettings(Graph.Get(), Boundary));
    }
    UPCGNode* Input = Graph->GetInputNode();
    UPCGSettings* OwnedSettings = Input->GetSettings();
    const FString Before = GetPCGGraphRevision(Graph.Get());
    const auto Payload = PointBoundaryPayload(TEXT("In"));
    FString Error;
    TestTrue(TEXT("Owned boundary settings can be staged"), ValidatePCGNodeSettings(Graph.Get(), Input, TEXT("set_pcg_node_settings"), Payload, Error));
    TestEqual(TEXT("Boundary preflight preserves authored state"), GetPCGGraphRevision(Graph.Get()), Before);
    FPCGEditState State(Graph.Get());
    int32 Count = 0;
    if (!TestTrue(TEXT("Point boundary settings imported"), ApplyPCGSettingsPayload(OwnedSettings, TEXT("set_pcg_node_settings"), Payload, Error, Count))) return false;
    Input->UpdateAfterSettingsChangeDuringCreation();
    OwnedSettings->PostEditChange();
    UPCGPin* Pin = Input->GetOutputPin(TEXT("In"));
    if (!TestNotNull(TEXT("Authored named point pin exists"), Pin)) return false;
    TestTrue(TEXT("Authored pin uses the native Point identifier"), Pin->Properties.AllowedTypes.IsSameType(FPCGDataTypeIdentifier(EPCGDataType::Point)));
    TestNotEqual(TEXT("Authored pin edit changes the revision"), GetPCGGraphRevision(Graph.Get()), Before);
    UClass* Class = ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings"));
    if (!TestNotNull(TEXT("Transform settings class"), Class)) return false;
    UPCGSettings* Settings = nullptr;
    UPCGNode* Transform = Graph->AddNodeOfType(Class, Settings);
    if (!TestNotNull(TEXT("Owned transform node"), Transform)) return false;
    TestTrue(TEXT("Ordinary private node settings remain editable"), HasOwnedPCGNodeSettings(Graph.Get(), Transform));
    Graph->AddLabeledEdge(Input, TEXT("In"), Transform, TEXT("In"));
    if (!TestTrue(TEXT("Connection actually exists"), HasPCGEdge(Pin, Transform->GetInputPin(TEXT("In"))))) return false;
    const FString Connected = GetPCGGraphRevision(Graph.Get());
    TestFalse(TEXT("Boundary edits cannot silently delete an existing connection"), ValidatePCGNodeSettings(Graph.Get(), Input,
        TEXT("set_pcg_node_settings"), PointBoundaryPayload(TEXT("Renamed")), Error));
    TestEqual(TEXT("Refused boundary edit preserves complete state"), GetPCGGraphRevision(Graph.Get()), Connected);
    auto Remove = MakeShared<FJsonObject>();
    Remove->SetStringField(TEXT("op"), TEXT("remove_node"));
    Remove->SetStringField(TEXT("nodeId"), Input->GetName());
    FPCGMutationContext Context;
    Context.bDestructive = true;
    TSharedPtr<FJsonObject> Result;
    FString Code;
    TestFalse(TEXT("Readable default IDs do not bypass protected-node removal"), ApplyPCGMutation(Graph.Get(), Remove, Context, Result, Error, Code));
    TestEqual(TEXT("Protected-node refusal preserves state"), GetPCGGraphRevision(Graph.Get()), Connected);
    const FObjectPropertyBase* InterfaceProperty = FindFProperty<FObjectPropertyBase>(UPCGNode::StaticClass(), TEXT("SettingsInterface"));
    if (!TestNotNull(TEXT("Reflected settings interface property"), InterfaceProperty)) return false;
    const auto CheckBorrowedSettings = [&](UPCGNode* Node, UPCGSettings* Borrowed, const TCHAR* Message)
    {
        // Inject only during the pure ownership check. The native setter destroys old settings.
        UObject* Original = InterfaceProperty->GetObjectPropertyValue_InContainer(Node);
        InterfaceProperty->SetObjectPropertyValue_InContainer(Node, Borrowed);
        const bool bOwned = HasOwnedPCGNodeSettings(Graph.Get(), Node);
        InterfaceProperty->SetObjectPropertyValue_InContainer(Node, Original);
        TestFalse(Message, bOwned);
    };
    CheckBorrowedSettings(Transform, OwnedSettings, TEXT("An ordinary node cannot borrow graph-owned boundary settings"));
    TStrongObjectPtr<UPCGGraph> Foreign(NewObject<UPCGGraph>(GetTransientPackage()));
    CheckBorrowedSettings(Input, Foreign->GetInputNode()->GetSettings(), TEXT("A boundary cannot edit another graph settings"));
    CheckBorrowedSettings(Input, Graph->GetOutputNode()->GetSettings(), TEXT("Input cannot borrow same-graph output settings"));
    TestTrue(TEXT("Restoring corresponding settings permits the native input again"), HasOwnedPCGNodeSettings(Graph.Get(), Input));
    TestTrue(TEXT("Explicit captured-state recovery restores boundary edits"), State.Restore());
    TestEqual(TEXT("Original boundary identity retained"), Graph->GetInputNode(), Input);
    TestEqual(TEXT("Original boundary settings identity retained"), Input->GetSettings(), OwnedSettings);
    TestEqual(TEXT("Original authored revision restored"), GetPCGGraphRevision(Graph.Get()), Before);
    return true;
}
#endif
