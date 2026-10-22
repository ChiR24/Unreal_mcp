#include "Misc/AutomationTest.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Parameters/McpPCGParameters.h"
#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG_PARAMETERS
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGNativeStructIdentityTest, "EngineWire.PCG.Closeout.ParameterStructIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGNativeStructIdentityTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("name"), TEXT("NativeValue"));
    Payload->SetStringField(TEXT("type"), TEXT("Struct"));
    for (UScriptStruct* Native : {TBaseStructure<FVector>::Get(), TBaseStructure<FRotator>::Get(), TBaseStructure<FTransform>::Get()})
    {
        Payload->SetStringField(TEXT("typeObject"), Native->GetPathName());
        FPropertyBagPropertyDesc Desc;
        FString Error;
        TestTrue(TEXT("An exact supported native struct is accepted"), BuildPCGParameterDesc(Payload, Desc, Error));
        TestTrue(TEXT("The descriptor retains the exact native struct identity"), Desc.ValueTypeObject.Get() == Native);
    }
    const FString PackageName = TEXT("/Temp/McpPCGCloseout_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TStrongObjectPtr<UPackage> Package(CreatePackage(*PackageName));
    TStrongObjectPtr<UScriptStruct> Impostor(NewObject<UScriptStruct>(Package.Get(), TEXT("Vector"), RF_Transient));
    Payload->SetStringField(TEXT("typeObject"), Impostor->GetPathName());
    FPropertyBagPropertyDesc Desc;
    FString Error;
    TestFalse(TEXT("A distinct same-named custom Vector struct is rejected"), BuildPCGParameterDesc(Payload, Desc, Error));
    TestTrue(TEXT("Rejection does not publish a parameter descriptor"), Desc.Name.IsNone());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGOuterTraversalBoundsTest, "EngineWire.PCG.Closeout.OuterTraversalIncludesIgnoredObjects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGOuterTraversalBoundsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>());
    FPCGReadBudget Small;
    TestTrue(TEXT("A small native graph fits the unchanged capture budget"), Small.CheckGraph(Graph.Get()));
    for (int32 Index = 0; Index < FPCGReadBudget::MaxObjects; ++Index)
        NewObject<UPCGNode>(Graph.Get(), NAME_None, RF_Transient);
    FPCGReadBudget Bounded;
    TestFalse(TEXT("Ignored transient children cannot escape the native traversal bound"), Bounded.CheckGraph(Graph.Get()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGQueryOffsetBoundsTest, "EngineWire.PCG.Closeout.MaximumQueryOffsetIsNotTruncated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGQueryOffsetBoundsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UEngineWireSubsystem* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("The editor bridge is available"), Bridge)) return false;
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("offset"), MAX_int32);
    Payload->SetNumberField(TEXT("limit"), 100);
    const FString Request = FGuid::NewGuid().ToString();
    FMcpResponseCaptureRegistry::Get().Begin(Request);
    const bool bHandled = HandlePCGDiscovery(Bridge, Request, TEXT("list_pcg_node_types"), Payload, nullptr);
    const auto Reply = FMcpResponseCaptureRegistry::Get().End(Request);
    if (!TestTrue(TEXT("The discovery handler produces its native reply"), bHandled && Reply.bCaptured && Reply.bSuccess && Reply.Result.IsValid())) return false;
    TestEqual(TEXT("An off-end native query has no rows"), Reply.Result->GetArrayField(TEXT("items")).Num(), 0);
    TestFalse(TEXT("An int32 maximum offset does not overflow the more-rows check"), Reply.Result->GetBoolField(TEXT("truncated")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGCoordinateStorageTest, "EngineWire.PCG.Closeout.NativeIntegerPositionCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGCoordinateStorageTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>());
    UPCGNode* Node = Graph->GetInputNode();
    if (!TestNotNull(TEXT("The graph owns a native input node"), Node)) return false;
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("x"), 12.9);
    Payload->SetNumberField(TEXT("y"), -8.75);
    ApplyNodeMetadata(Node, Payload);
    int32 X = 0, Y = 0;
    Node->GetNodePosition(X, Y);
    TestEqual(TEXT("Standalone metadata uses native int32 coordinates"), X, 12);
    TestEqual(TEXT("Negative coordinates truncate toward zero"), Y, -8);
    Payload->SetStringField(TEXT("op"), TEXT("move_node"));
    Payload->SetStringField(TEXT("nodeId"), Node->GetName());
    FPCGMutationContext Context;
    TSharedPtr<FJsonObject> Result;
    FString Error, Code;
    TestTrue(TEXT("The batch path retains the same established coordinate conversion"), ApplyPCGMutation(Graph.Get(), Payload, Context, Result, Error, Code));
    Node->GetNodePosition(X, Y);
    TestEqual(TEXT("Batch and standalone agree on x"), X, 12);
    TestEqual(TEXT("Batch and standalone agree on y"), Y, -8);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGNodeLocatorCompatibilityTest, "EngineWire.PCG.Closeout.ExistingNodeLocatorPreservesTitle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGNodeLocatorCompatibilityTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    UEngineWireSubsystem* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("The editor bridge is available"), Bridge)) return false;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>());
    UPCGSettings* Settings = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(ResolvePCGSettingsClass(TEXT("PCGTransformPointsSettings")), Settings);
    if (!TestTrue(TEXT("A native settings node exists"), Node && Settings)) return false;
    Node->NodeTitle = FName(TEXT("AuthoredDisplayTitle"));
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("nodeName"), Node->GetName());
    auto Values = MakeShared<FJsonObject>();
    Values->SetNumberField(TEXT("Seed"), 101);
    Payload->SetObjectField(TEXT("settings"), Values);
    const FString Request = FGuid::NewGuid().ToString();
    FMcpResponseCaptureRegistry::Get().Begin(Request);
    HandleSetPCGNodeSettings(Bridge, Request, TEXT("set_pcg_node_settings"), Payload, nullptr, false, Graph.Get(), Graph->GetPathName());
    const auto Edited = FMcpResponseCaptureRegistry::Get().End(Request);
    if (!TestTrue(TEXT("A settings edit addressed by nodeName succeeds"), Edited.bCaptured && Edited.bSuccess)) return false;
    TestEqual(TEXT("nodeName locates the node without replacing its authored title"), Node->NodeTitle, FName(TEXT("AuthoredDisplayTitle")));
    TestEqual(TEXT("The requested settings change still applies"), Settings->Seed, 101);
    const FString BeforeEmptyEdit = GetPCGGraphRevision(Graph.Get());
    Payload->RemoveField(TEXT("settings"));
    const FString EmptyRequest = FGuid::NewGuid().ToString();
    FMcpResponseCaptureRegistry::Get().Begin(EmptyRequest);
    HandleSetPCGNodeSettings(Bridge, EmptyRequest, TEXT("set_pcg_node_settings"), Payload, nullptr, false, Graph.Get(), Graph->GetPathName());
    const auto Refused = FMcpResponseCaptureRegistry::Get().End(EmptyRequest);
    TestTrue(TEXT("An empty edit is refused, not reported updated"), Refused.bCaptured && !Refused.bSuccess);
    TestEqual(TEXT("The upstream empty-edit refusal is preserved"), Refused.ErrorCode, FString(TEXT("INVALID_ARGUMENT")));
    TestEqual(TEXT("The refusal preserves the entire authored graph"), GetPCGGraphRevision(Graph.Get()), BeforeEmptyEdit);
    return true;
}
#endif
