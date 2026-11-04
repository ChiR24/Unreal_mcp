#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpGraphCodeSelfCallTest, "EngineWire.BlueprintGraph.Code.SelfCall",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// A call to one of the Blueprint's own functions, written with two arguments and read back. On self its target (the
// impure call's visible self pin) stays unwired and the arguments fill A and B; the writer used to give the first argument
// to the target, which refused "3". On a target the code names, the target takes the first argument, and the reader keeps
// every argument after it, a trailing default included, so the text it prints writes back the same.
bool FMcpGraphCodeSelfCallTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpGraphCode;
    // An Actor Blueprint of its own, in memory only, with a function AddTwo(A, B) whose call is on the action menu.
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/McpGraphCodeSelf_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_SelfCall"), BPTYPE_Normal,
                                                                    UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Events = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    if (!TestNotNull(TEXT("an Actor Blueprint with an event graph"), Events))
    {
        Package->MarkAsGarbage();
        return false;
    }
    UEdGraph* Function = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("AddTwo"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Function, true, nullptr);
    FEdGraphPinType Int;
    Int.PinCategory = UEdGraphSchema_K2::PC_Int;
    for (UEdGraphNode* Node : Function->Nodes)
    {
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
        {
            Entry->CreateUserDefinedPin(TEXT("A"), Int, EGPD_Output);
            Entry->CreateUserDefinedPin(TEXT("B"), Int, EGPD_Output);
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    FBlueprintActionDatabase::Get().RefreshAssetActions(Blueprint);
    // The call's id as the event graph's menu offers it (CallFunction|AddTwo), the id read_graph_as_code prints.
    const TSet<FString> Title = {MenuTitleKey(TEXT("AddTwo"))};
    const TArray<FMenuEntry> Offered = MenuEntries(Blueprint, Events, MenuCandidates(Title), Title);
    TestTrue(TEXT("the event graph's menu offers the call to AddTwo"), Offered.Num() > 0);
    const FString Call = Offered.Num() > 0 ? Offered[0].Id : FString(TEXT("CallFunction|AddTwo"));

    // Written as write_graph_from_code writes it: every graph planned, then built, the event graph first.
    const FString Code = FString::Printf(TEXT("(fn AddTwo (A B)\n  (Development|PrintString \"added\"))\n(event Custom|OnSelf\n  (%s 3 4))\n"
                                              "(event Custom|OnTarget\n  (%s self 3 0))"),
                                         *Call, *Call);
    TArray<FSExpr> Forms;
    FString Error;
    TestTrue(TEXT("the code parses"), ParseSExpr(Code, Forms, Error) && Forms.Num() == 3);
    TArray<Write::FGraphPlan> Plans;
    Plans.AddDefaulted(2);
    Plans[0].Graph = Events;
    Plans[0].Forms = {1, 2};
    Plans[1].Graph = Function;
    Plans[1].Forms = {0};
    const TSet<FString> Titles = Write::CodeTitles(Forms);
    TArray<FMenuEntry> Candidates = MenuCandidates(Titles);
    TMap<UEdGraphNode*, int32> Made;
    TArray<TSharedPtr<FJsonValue>> Rows;
    bool bWritten = Forms.Num() == 3 && Write::PlanGraph(Forms, Plans[0], Error) && Write::PlanGraph(Forms, Plans[1], Error);
    for (Write::FGraphPlan& Plan : Plans)
    {
        bWritten = bWritten && Write::BuildForms(Blueprint, Forms, Candidates, Titles, Plan, Made, Rows, Error);
    }
    FString CompileError;
    TestTrue(FString::Printf(TEXT("the code is written (%s)"), *Error), bWritten);
    TestTrue(FString::Printf(TEXT("and compiles (%s)"), *CompileError), bWritten && McpSafeCompileBlueprint(Blueprint, &CompileError));

    // The wiring: on self the target is unwired and A, B hold 3 and 4; on the named target a self node is wired in, A, B 3 and 0.
    int32 OnSelf = 0;
    int32 OnTarget = 0;
    for (UEdGraphNode* Node : Events->Nodes)
    {
        UK2Node_CallFunction* Each = Cast<UK2Node_CallFunction>(Node);
        const bool bAddTwo = Each && Each->FunctionReference.GetMemberName() == FName(TEXT("AddTwo"));
        UEdGraphPin* Target = bAddTwo ? Each->FindPin(UEdGraphSchema_K2::PN_Self) : nullptr;
        UEdGraphPin* A = bAddTwo ? Each->FindPin(TEXT("A")) : nullptr;
        UEdGraphPin* B = bAddTwo ? Each->FindPin(TEXT("B")) : nullptr;
        if (!Target || !A || !B)
        {
            continue;
        }
        const bool bSelfNode = Target->LinkedTo.Num() == 1 && Target->LinkedTo[0]->GetOwningNode()->IsA<UK2Node_Self>();
        OnSelf += Target->LinkedTo.Num() == 0 && Target->DefaultValue.IsEmpty() && A->DefaultValue == TEXT("3") && B->DefaultValue == TEXT("4") ? 1 : 0;
        OnTarget += bSelfNode && A->DefaultValue == TEXT("3") && B->DefaultValue == TEXT("0") ? 1 : 0;
    }
    TestEqual(TEXT("on self: the target left unwired, the two arguments in A and B"), OnSelf, 1);
    TestEqual(TEXT("on a target the code names: the target wired, then A and B"), OnTarget, 1);

    // The text read back: the target left out on self, and first, with every argument after it, where it is named.
    const TSharedPtr<FJsonObject> Read = MakeShared<FJsonObject>();
    TArray<FString> Names;
    ReadAsCode(Blueprint, {Events}, {TEXT("Custom|OnSelf"), TEXT("Custom|OnTarget")}, true, *Read, Names);
    FString Text;
    Read->TryGetStringField(TEXT("code"), Text);
    TestTrue(FString::Printf(TEXT("on self the call reads back as written:\n%s"), *Text), Text.Contains(FString::Printf(TEXT("(%s 3 4)"), *Call)));
    TestTrue(TEXT("on a named target it reads back as written, its default 0 kept"), Text.Contains(FString::Printf(TEXT("(%s self 3 0)"), *Call)));

    FBlueprintActionDatabase::Get().ClearAssetActions(Blueprint);
    Blueprint->ClearFlags(RF_Public | RF_Standalone);
    Blueprint->MarkAsGarbage();
    Package->MarkAsGarbage();
    return true;
}
#endif
