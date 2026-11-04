#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"

namespace McpGraphCode
{
namespace Write
{
namespace WriteNodeParts
{
// A loop or gate the action menu does not name: the StandardMacros graph of that title.
UEdGraphNode* StandardMacro(UEdGraph* Graph, const FString& TitleKey)
{
    UBlueprint* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    UEdGraph* Macro = nullptr;
    if (!Macros)
    {
        return nullptr;
    }
    for (UEdGraph* Each : Macros->MacroGraphs)
    {
        if (Each && MenuTitleKey(Each->GetName()) == TitleKey)
        {
            Macro = Each;
            break;
        }
    }
    if (!Macro)
    {
        return nullptr;
    }
    FGraphNodeCreator<UK2Node_MacroInstance> Creator(*Graph);
    UK2Node_MacroInstance* Node = Creator.CreateNode(false);
    Node->SetMacroGraph(Macro);
    Creator.Finalize();
    return Node;
}
} // namespace WriteNodeParts

using namespace WriteNodeParts;

TArray<UEdGraphPin*> DataPins(UEdGraphNode* Node, bool bInputs)
{
    TArray<UEdGraphPin*> Pins;
    if (!Node)
    {
        return Pins;
    }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && !Pin->bHidden && (Pin->Direction == EGPD_Input) == bInputs && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
            !(Pin->Direction == EGPD_Output && Pin->PinName == TEXT("OutputDelegate")))
        {
            Pins.Add(Pin);
        }
    }
    return Pins;
}

UEdGraphPin* DataPin(UEdGraphNode* Node, bool bInput, const TCHAR* Name)
{
    for (UEdGraphPin* Pin : DataPins(Node, bInput))
    {
        if (Pin->PinName == Name)
        {
            return Pin;
        }
    }
    return nullptr;
}

UEdGraphPin* ExecPin(UEdGraphNode* Node, bool bInput, const FString& Name)
{
    if (!Node)
    {
        return nullptr;
    }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && !Pin->bHidden && (Pin->Direction == EGPD_Input) == bInput && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            (Name.IsEmpty() || Pin->PinName.ToString().Equals(Name, ESearchCase::CaseSensitive)))
        {
            return Pin;
        }
    }
    return nullptr;
}

bool FTranspiler::Fail(const FString& Message)
{
    Error = Line > 0 ? FString::Printf(TEXT("Line %d: %s"), Line, *Message) : Message;
    return false;
}

// Epic's create_node: a named custom event, else the menu entry with that id (FindMenuEntry: any case when no exact one,
// a value-giving function first for a value), else a StandardMacros graph of that title.
UEdGraphNode* FTranspiler::Create(const FString& TypeId, bool bValue)
{
    if (UEdGraphNode* Reused = Reuse(TypeId))
    {
        return Reused;
    }
    UEdGraphNode* Node = nullptr;
    const FString CustomEvent = TEXT("AddEvent|Custom|");
    if (TypeId.StartsWith(CustomEvent, ESearchCase::CaseSensitive))
    {
        FGraphNodeCreator<UK2Node_CustomEvent> Creator(*Graph);
        UK2Node_CustomEvent* Event = Creator.CreateNode(false);
        Event->CustomFunctionName = FName(*TypeId.Mid(CustomEvent.Len()));
        Creator.Finalize();
        Node = Event;
    }
    else
    {
        const FMenuEntry* Found = FindMenuEntry(Menu, TypeId, bValue);
        if (Found)
        {
            const FBlueprintActionInfo Info(Found->Owner, Found->Spawner);
            Node = Found->Spawner->Invoke(Graph, Info.GetBindings(), FVector2D::ZeroVector);
        }
        Node = Node ? Node : StandardMacro(Graph, MenuTitleKey(TypeId));
    }
    if (!Node)
    {
        const TArray<FString> Offered = OfferedIds(Blueprint, Graph, Menu, TypeId);
        Fail(Offered.Num() > 0 ? FString::Printf(TEXT("the menu of graph %s has no %s; it offers %s."), *Graph->GetName(), *TypeId, *FString::Join(Offered, TEXT(", ")))
                               : FString::Printf(TEXT("the menu of graph %s has no node %s (a type id is Category|Title, spaces removed, as read_graph_as_code prints it)."), *Graph->GetName(), *TypeId));
        return nullptr;
    }
    Made.Add(Node, Line);
    return Node;
}

bool FTranspiler::Connect(UEdGraphPin* Out, UEdGraphPin* In)
{
    if (Out && In && Graph->GetSchema()->TryCreateConnection(Out, In))
    {
        return true;
    }
    return Fail(FString::Printf(TEXT("%s.%s cannot be wired to %s.%s."), Out ? *Out->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("?"),
                                Out ? *Out->PinName.ToString() : TEXT("?"), In ? *In->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("?"),
                                In ? *In->PinName.ToString() : TEXT("?")));
}

// Every exec output the flow waits on into the node's exec input; false when it has none or nothing waits.
bool FTranspiler::WireExec(UEdGraphNode* Node, FFlow& Flow)
{
    UEdGraphPin* In = ExecPin(Node, true, FString());
    if (In && !TagForExec.IsEmpty())
    {
        Tagged.FindOrAdd(TagForExec) = Node;
        TagForExec.Reset();
    }
    if (!In || Flow.Pending.Num() == 0)
    {
        return false;
    }
    for (UEdGraphPin* Out : Flow.Pending)
    {
        if (!Connect(Out, In))
        {
            return false;
        }
    }
    return true;
}

UEdGraphPin* FTranspiler::Bound(const FString& Name, const FBindings& Bindings)
{
    if (UEdGraphPin* const* Pin = Bindings.Find(Name))
    {
        return *Pin;
    }
    TArray<FString> Names;
    Bindings.GetKeys(Names);
    Names.Sort();
    Fail(Name.StartsWith(TEXT("/")) ? FString::Printf(TEXT("\"%s\" looks like a class path or asset reference: quote it, \"%s\"."), *Name, *Name)
                                     : FString::Printf(TEXT("Undefined variable \"%s\". Available: %s."), *Name, *FString::Join(Names, TEXT(", "))));
    return nullptr;
}

bool FTranspiler::WireArg(const FSExpr& Value, UEdGraphPin* Pin, const FBindings& Bindings, FFlow& Flow)
{
    if (Value.Type == FSExpr::EType::Symbol)
    {
        UEdGraphPin* Source = Bound(Value.Text, Bindings);
        return Source && Connect(Source, Pin);
    }
    if (Value.Type == FSExpr::EType::List)
    {
        UEdGraphPin* Out = nullptr;
        return Expr(Value, Bindings, Flow, Out) && (!Out || Connect(Out, Pin));
    }
    // A literal into a pin with no type yet goes through a Make Literal node; others take it as the pin's value.
    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
    {
        UEdGraphPin* Literal = MakeLiteral(Value);
        return Literal && Connect(Literal, Pin);
    }
    return SetValue(Pin, LiteralText(Value));
}

bool FTranspiler::WireWithPre(const FSExpr& Value, UEdGraphPin* Pin, UEdGraphPin* Pre, const FBindings& Bindings, FFlow& Flow)
{
    if (Value.Type == FSExpr::EType::List)
    {
        return !Pre || Connect(Pre, Pin);
    }
    return WireArg(Value, Pin, Bindings, Flow);
}
} // namespace Write
} // namespace McpGraphCode
