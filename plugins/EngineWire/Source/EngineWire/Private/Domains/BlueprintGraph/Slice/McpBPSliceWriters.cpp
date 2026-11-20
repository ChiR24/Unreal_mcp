// Writer rows of a slice: the entries that reach a node, the Branches it sits behind, the callers of its function.

#include "Domains/BlueprintGraph/Slice/McpBPSliceWriters.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"

namespace McpBPSliceDetail
{
using McpBPLint::IsExecPin;

struct FEntry
{
    const UBlueprint* Blueprint = nullptr;
    const UEdGraphNode* Node = nullptr;
    FName Name;     // the function or custom event a call names; NAME_None for an engine event
    FString Kind;   // function, custom event, event, entry
};

TSharedPtr<FJsonObject> NodeRow(const UBlueprint* Blueprint, const UEdGraphNode* Node)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("blueprint"), Blueprint->GetOutermost()->GetName());
    Row->SetStringField(TEXT("graph"), GetNameSafe(Node->GetGraph()));
    Row->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
    Row->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    return Row;
}

bool SameBlueprintChain(const UClass* Class, const UBlueprint* Owner)
{
    for (; Class; Class = Class->GetSuperClass())
    {
        if (UBlueprint::GetBlueprintFromClass(Class) == Owner)
        {
            return true;
        }
    }
    return false;
}

// The entries an exec path from reaches Node: events, function entries, a collapsed graph's tunnel.
TArray<const UEdGraphNode*> EntriesOf(const UEdGraphNode* Node)
{
    TArray<const UEdGraphNode*> Stack{Node}, Entries;
    TSet<const UEdGraphNode*> Seen{Node};
    while (Stack.Num() > 0)
    {
        const UEdGraphNode* At = Stack.Pop();
        if (McpBPLint::IsFlowEntry(At))
        {
            Entries.Add(At);
            continue;
        }
        for (const UEdGraphPin* Pin : At->Pins)
        {
            if (!IsExecPin(Pin) || Pin->Direction != EGPD_Input)
            {
                continue;
            }
            for (const UEdGraphPin* From : Pin->LinkedTo)
            {
                bool bSeen = true;
                if (From)
                {
                    Seen.Add(From->GetOwningNode(), &bSeen);
                }
                if (!bSeen)
                {
                    Stack.Add(From->GetOwningNode());
                }
            }
        }
    }
    return Entries;
}

// Whether an exec path from Exit reaches Target.
bool Reaches(const UEdGraphPin* Exit, const UEdGraphNode* Target)
{
    TArray<const UEdGraphPin*> Stack;
    if (Exit)
    {
        Stack.Add(Exit);
    }
    TSet<const UEdGraphNode*> Seen;
    while (Stack.Num() > 0)
    {
        for (const UEdGraphPin* Linked : Stack.Pop()->LinkedTo)
        {
            const UEdGraphNode* Next = Linked ? Linked->GetOwningNode() : nullptr;
            bool bSeen = true;
            if (Next)
            {
                Seen.Add(Next, &bSeen);
            }
            if (Next == Target)
            {
                return true;
            }
            if (bSeen)
            {
                continue;
            }
            for (const UEdGraphPin* Pin : Next->Pins)
            {
                if (IsExecPin(Pin) && Pin->Direction == EGPD_Output)
                {
                    Stack.Add(Pin);
                }
            }
        }
    }
    return false;
}

// What a Branch's condition reads: a variable's name, a node's output, or the literal it is left at.
FString ConditionText(const UK2Node_IfThenElse* Branch)
{
    const UEdGraphPin* Condition = Branch->GetConditionPin();
    const UEdGraphPin* Source = McpBPLint::SourceOf(Condition);
    if (!Source)
    {
        return Condition ? Condition->DefaultValue : FString();
    }
    if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Source->GetOwningNode()))
    {
        return Get->GetVarName().ToString();
    }
    return Source->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".") + Source->PinName.ToString();
}

// Each Branch every path to Node passes, with the exit that leads on to it (a Branch both exits of which lead there
// decides nothing).
TArray<TSharedPtr<FJsonValue>> ConditionsOf(const UEdGraphNode* Node, TMap<const UEdGraph*, McpBPLint::FFlow>& Flows)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    const UEdGraph* Graph = Node->GetGraph();
    if (!Flows.Contains(Graph))
    {
        Flows.Add(Graph, McpBPLint::BuildFlow(Graph));
    }
    const McpBPLint::FFlow& Flow = Flows[Graph];
    TMap<int32, const UEdGraphNode*> ByVertex;
    for (const TPair<const UEdGraphNode*, int32>& Pair : Flow.Vertex)
    {
        ByVertex.Add(Pair.Value, Pair.Key);
    }
    const int32* At = Flow.Vertex.Find(Node);
    for (int32 Up = At ? Flow.Idom[*At] : INDEX_NONE; Up > 0; Up = Flow.Idom[Up])
    {
        const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(ByVertex.FindRef(Up));
        const bool bThen = Branch && Reaches(Branch->GetThenPin(), Node);
        if (!Branch || bThen == Reaches(Branch->GetElsePin(), Node))
        {
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("nodeId"), Branch->NodeGuid.ToString());
        Row->SetStringField(TEXT("condition"), ConditionText(Branch));
        Row->SetBoolField(TEXT("mustBe"), bThen);
        Rows.Insert(MakeShared<FJsonValueObject>(Row), 0); // outermost first
    }
    return Rows;
}

FEntry EntryOf(const UBlueprint* Blueprint, const UEdGraphNode* Node)
{
    FEntry Entry{Blueprint, Node, NAME_None, TEXT("entry")};
    if (const UK2Node_CustomEvent* Custom = Cast<UK2Node_CustomEvent>(Node))
    {
        Entry.Name = Custom->CustomFunctionName;
        Entry.Kind = TEXT("custom event");
    }
    else if (Cast<UK2Node_Event>(Node))
    {
        Entry.Kind = TEXT("event");
    }
    else if (Cast<UK2Node_FunctionEntry>(Node))
    {
        Entry.Name = Node->GetGraph()->GetFName();
        Entry.Kind = TEXT("function");
    }
    return Entry;
}

// The calls in scope to the function or custom event Entry starts: a call by its name on Entry's Blueprint or a parent
// it overrides.
TArray<TSharedPtr<FJsonValue>> CallersOf(const FEntry& Entry, const TArray<UBlueprint*>& Scope)
{
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const UBlueprint* Blueprint : Scope)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph)
            {
                continue;
            }
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                const UFunction* Function = Call && Call->FunctionReference.GetMemberName() == Entry.Name ? Call->GetTargetFunction() : nullptr;
                const UClass* Declaring = Function ? Function->GetOwnerClass() : nullptr;
                const UBlueprint* DeclaringBlueprint = Declaring ? UBlueprint::GetBlueprintFromClass(Declaring) : nullptr;
                if (DeclaringBlueprint && SameBlueprintChain(Entry.Blueprint->GeneratedClass, DeclaringBlueprint) && Rows.Num() < MaxRows)
                {
                    Rows.Add(MakeShared<FJsonValueObject>(NodeRow(Blueprint, Node)));
                }
            }
        }
    }
    return Rows;
}

TSharedPtr<FJsonObject> WriterRow(const UBlueprint* Blueprint, const UEdGraphNode* Node, const TArray<UBlueprint*>& Scope,
                                  TMap<const UEdGraph*, McpBPLint::FFlow>& Flows)
{
    TSharedPtr<FJsonObject> Row = NodeRow(Blueprint, Node);
    TArray<TSharedPtr<FJsonValue>> Entries;
    for (const UEdGraphNode* EntryNode : EntriesOf(Node))
    {
        const FEntry Entry = EntryOf(Blueprint, EntryNode);
        TSharedPtr<FJsonObject> EntryRow = NodeRow(Blueprint, EntryNode);
        EntryRow->SetStringField(TEXT("kind"), Entry.Kind);
        if (!Entry.Name.IsNone())
        {
            EntryRow->SetArrayField(TEXT("calledFrom"), CallersOf(Entry, Scope));
        }
        Entries.Add(MakeShared<FJsonValueObject>(EntryRow));
    }
    Row->SetArrayField(TEXT("entries"), Entries);
    const TArray<TSharedPtr<FJsonValue>> Conditions = ConditionsOf(Node, Flows);
    if (Conditions.Num() > 0)
    {
        Row->SetArrayField(TEXT("conditions"), Conditions);
    }
    return Row;
}
} // namespace McpBPSliceDetail
