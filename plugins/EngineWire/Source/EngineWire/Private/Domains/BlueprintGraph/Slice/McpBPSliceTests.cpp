#include "Domains/BlueprintGraph/Slice/McpBPSlice.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBPSliceTest, "EngineWire.BlueprintGraph.Slice",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace McpBPSliceTestDetail
{
template <typename NodeType>
NodeType* Make(UEdGraph& Graph, TFunctionRef<void(NodeType&)> Setup)
{
    FGraphNodeCreator<NodeType> Creator(Graph);
    NodeType* Node = Creator.CreateNode();
    Setup(*Node);
    Creator.Finalize();
    return Node;
}

const TSharedPtr<FJsonObject>* Row(const TSharedPtr<FJsonObject>& Slice, const TCHAR* List, int32 Index)
{
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    const TSharedPtr<FJsonObject>* Out = nullptr;
    return Slice->TryGetArrayField(List, Rows) && Rows->IsValidIndex(Index) && (*Rows)[Index]->TryGetObject(Out) ? Out : nullptr;
}
} // namespace McpBPSliceTestDetail

// A door: OnKey -> Branch(bHasKey) -> true -> Set bOpen; OnReset -> Set bOpen, called once from OnKey's false exit. bOpen
// is set twice and read nowhere (a dead write), bHasKey read once and never set.
bool FMcpBPSliceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpBPSliceTestDetail;
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/McpBPSlice_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, TEXT("BP_SliceDoor"), BPTYPE_Normal,
                                                                    UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Events = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
    if (!TestNotNull(TEXT("an Actor Blueprint with an event graph"), Events))
    {
        Package->MarkAsGarbage();
        return false;
    }
    FEdGraphPinType Bool;
    Bool.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bOpen"), Bool);
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bHasKey"), Bool);
    UK2Node_CustomEvent* OnKey = Make<UK2Node_CustomEvent>(*Events, [](UK2Node_CustomEvent& Node) { Node.CustomFunctionName = TEXT("OnKey"); });
    UK2Node_CustomEvent* OnReset = Make<UK2Node_CustomEvent>(*Events, [](UK2Node_CustomEvent& Node) { Node.CustomFunctionName = TEXT("OnReset"); });
    UK2Node_IfThenElse* Branch = Make<UK2Node_IfThenElse>(*Events, [](UK2Node_IfThenElse&) {});
    UK2Node_VariableGet* HasKey = Make<UK2Node_VariableGet>(*Events, [](UK2Node_VariableGet& Node) { Node.VariableReference.SetSelfMember(TEXT("bHasKey")); });
    UK2Node_VariableSet* Open = Make<UK2Node_VariableSet>(*Events, [](UK2Node_VariableSet& Node) { Node.VariableReference.SetSelfMember(TEXT("bOpen")); });
    UK2Node_VariableSet* Reset = Make<UK2Node_VariableSet>(*Events, [](UK2Node_VariableSet& Node) { Node.VariableReference.SetSelfMember(TEXT("bOpen")); });
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    UFunction* ResetFunction = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass->FindFunctionByName(TEXT("OnReset")) : nullptr;
    TestNotNull(TEXT("OnReset compiles to a function a call can name"), ResetFunction);
    UK2Node_CallFunction* CallReset = Make<UK2Node_CallFunction>(*Events, [ResetFunction](UK2Node_CallFunction& Node) { Node.SetFromFunction(ResetFunction); });

    OnKey->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Branch->GetExecPin());
    HasKey->GetValuePin()->MakeLinkTo(Branch->GetConditionPin());
    Branch->GetThenPin()->MakeLinkTo(Open->GetExecPin());
    Branch->GetElsePin()->MakeLinkTo(CallReset->GetExecPin());
    OnReset->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Reset->GetExecPin());

    bool bSet = false;
    TestTrue(TEXT("a Set of bOpen touches bOpen as a write"), McpBPSlice::Touches(Open, Blueprint, TEXT("bOpen"), bSet) && bSet);
    TestTrue(TEXT("a Get of bHasKey touches bHasKey as a read"), McpBPSlice::Touches(HasKey, Blueprint, TEXT("bHasKey"), bSet) && !bSet);
    TestFalse(TEXT("a Get of bHasKey does not touch bOpen"), McpBPSlice::Touches(HasKey, Blueprint, TEXT("bOpen"), bSet));

    const TSharedPtr<FJsonObject> OpenSlice = McpBPSlice::Slice(Blueprint, TEXT("bOpen"), {Blueprint});
    TestEqual(TEXT("bOpen has two writers"), int32(OpenSlice->GetNumberField(TEXT("writerCount"))), 2);
    TestEqual(TEXT("and no reader"), int32(OpenSlice->GetNumberField(TEXT("readerCount"))), 0);
    TestTrue(TEXT("so it is a dead write"), OpenSlice->HasField(TEXT("deadWrite")));
    int32 Guarded = 0, Called = 0;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const TSharedPtr<FJsonObject>* Writer = Row(OpenSlice, TEXT("writers"), Index);
        const TSharedPtr<FJsonObject>* Condition = Writer ? Row(*Writer, TEXT("conditions"), 0) : nullptr;
        if (Condition && (*Condition)->GetStringField(TEXT("condition")) == TEXT("bHasKey") && (*Condition)->GetBoolField(TEXT("mustBe")))
        {
            ++Guarded;
        }
        const TSharedPtr<FJsonObject>* Entry = Writer ? Row(*Writer, TEXT("entries"), 0) : nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Callers = nullptr;
        if (Entry && (*Entry)->GetStringField(TEXT("kind")) == TEXT("custom event") && (*Entry)->TryGetArrayField(TEXT("calledFrom"), Callers) && Callers->Num() == 1)
        {
            ++Called;
        }
    }
    TestEqual(TEXT("one writer sits behind the Branch on bHasKey, its true exit"), Guarded, 1);
    TestEqual(TEXT("the writer in OnReset lists OnReset's one call; nothing calls OnKey"), Called, 1);

    const TSharedPtr<FJsonObject> KeySlice = McpBPSlice::Slice(Blueprint, TEXT("bHasKey"), {Blueprint});
    TestEqual(TEXT("bHasKey has one reader"), int32(KeySlice->GetNumberField(TEXT("readerCount"))), 1);
    TestEqual(TEXT("and no writer"), int32(KeySlice->GetNumberField(TEXT("writerCount"))), 0);
    TestFalse(TEXT("read but never set is no dead write"), KeySlice->HasField(TEXT("deadWrite")));

    Blueprint->ClearFlags(RF_Public | RF_Standalone);
    Blueprint->MarkAsGarbage();
    Package->MarkAsGarbage();
    return true;
}
#endif
