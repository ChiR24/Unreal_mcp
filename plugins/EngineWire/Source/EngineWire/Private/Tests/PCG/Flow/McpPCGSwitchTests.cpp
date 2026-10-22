#include "Misc/AutomationTest.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"

#if WITH_DEV_AUTOMATION_TESTS && MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Elements/ControlFlow/PCGSwitch.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpPCGSwitchOptionsTest, "EngineWire.PCG.Flow.SwitchOptionsRefreshNativeCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMcpPCGSwitchOptionsTest::RunTest(const FString& Parameters)
{
    using namespace McpPCGHandlers;
    TStrongObjectPtr<UPCGGraph> Graph(NewObject<UPCGGraph>(GetTransientPackage()));
    UPCGSettings* Base = nullptr;
    UPCGNode* Node = Graph->AddNodeOfType(UPCGSwitchSettings::StaticClass(), Base);
    auto* Settings = Cast<UPCGSwitchSettings>(Base);
    if (!TestTrue(TEXT("A real native Switch fixture is initialized"), Node && Settings)) return false;
    Settings->UpdateSettings();
    TestTrue(TEXT("The fixture starts with the native integer option cache"), Settings->CachedPinLabels.Contains(FName(TEXT("0"))));

    auto Fields = MakeShared<FJsonObject>();
    Fields->SetStringField(TEXT("SelectionMode"), TEXT("String"));
    Fields->SetArrayField(TEXT("StringOptions"), {MakeShared<FJsonValueString>(TEXT("MCP_Switch_A")),
        MakeShared<FJsonValueString>(TEXT("MCP_Switch_B"))});
    Fields->SetStringField(TEXT("StringSelection"), TEXT("MCP_Switch_B"));
    FString Error;
    int32 Applied = 0;
    if (!TestTrue(TEXT("The public settings writer accepts native String options"), ApplySettingsObject(Settings, Fields, Error, Applied)))
    {
        AddError(Error);
        return false;
    }
    Node->UpdateAfterSettingsChangeDuringCreation();
    Settings->PostEditChange();
    TestEqual(TEXT("All three authored settings were applied"), Applied, 3);
    TestNotNull(TEXT("The actual output interface contains option A"), Node->GetOutputPin(FName(TEXT("MCP_Switch_A"))));
    TestNotNull(TEXT("The actual output interface contains option B"), Node->GetOutputPin(FName(TEXT("MCP_Switch_B"))));
    TestEqual(TEXT("The execution cache contains both options and the native fallback"), Settings->CachedPinLabels.Num(), 3);
    TestTrue(TEXT("The execution cache follows authored option A"), Settings->CachedPinLabels.Contains(FName(TEXT("MCP_Switch_A"))));
    TestTrue(TEXT("The execution cache follows authored option B"), Settings->CachedPinLabels.Contains(FName(TEXT("MCP_Switch_B"))));
    TestFalse(TEXT("The retired integer option is not an execution-cache member"), Settings->CachedPinLabels.Contains(FName(TEXT("0"))));

    const FString Revision = GetPCGGraphRevision(Graph.Get());
    if (!TestFalse(TEXT("A complete authored revision exists"), Revision.IsEmpty())) return false;
    const TArray<FName> CachedBefore = Settings->CachedPinLabels;
    auto Invalid = MakeShared<FJsonObject>();
    Invalid->SetArrayField(TEXT("StringOptions"), {MakeShared<FJsonValueString>(TEXT("MCP_Switch_C"))});
    Invalid->SetBoolField(TEXT("MCP_UnknownProperty"), true);
    auto Payload = MakeShared<FJsonObject>();
    Payload->SetObjectField(TEXT("settings"), Invalid);
    TestFalse(TEXT("Mixed invalid settings remain refused on staged data"),
        ValidatePCGNodeSettings(Graph.Get(), Node, TEXT("set_pcg_node_settings"), Payload, Error));
    TestEqual(TEXT("Failed preflight preserves authored state"), GetPCGGraphRevision(Graph.Get()), Revision);
    TestTrue(TEXT("Failed preflight preserves the original execution cache"), Settings->CachedPinLabels == CachedBefore);
    return true;
}
#endif
