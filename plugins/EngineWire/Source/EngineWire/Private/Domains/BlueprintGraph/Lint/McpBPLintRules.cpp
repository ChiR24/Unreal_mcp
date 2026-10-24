#include "Domains/BlueprintGraph/Lint/McpBPLintRules.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpBPLint
{
namespace
{
bool IsExec(const UEdGraphPin* Pin)
{
    return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

bool HasExec(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
    return Node->Pins.ContainsByPredicate([Direction](const UEdGraphPin* Pin) { return IsExec(Pin) && Pin->Direction == Direction; });
}

// Every node the exec flow reaches from Starts, Starts included.
TSet<const UEdGraphNode*> Flood(const TArray<const UEdGraphNode*>& Starts)
{
    TSet<const UEdGraphNode*> Seen;
    TArray<const UEdGraphNode*> Open = Starts;
    for (const UEdGraphNode* Start : Starts)
    {
        Seen.Add(Start);
    }
    while (Open.Num() > 0)
    {
        const UEdGraphNode* Node = Open.Pop();
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!IsExec(Pin) || Pin->Direction != EGPD_Output)
            {
                continue;
            }
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                const UEdGraphNode* Next = Linked ? Linked->GetOwningNode() : nullptr;
                if (Next && !Seen.Contains(Next))
                {
                    Seen.Add(Next);
                    Open.Add(Next);
                }
            }
        }
    }
    return Seen;
}

FName FunctionName(const UEdGraphNode* Node)
{
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
    return Call && Call->GetTargetFunction() ? Call->GetTargetFunction()->GetFName() : NAME_None;
}
} // namespace

// Event Tick, and the pure nodes feeding what it runs, reaching a level search or a spawn: every frame.
void TickHeavy(const UEdGraph* Graph, TArray<FFinding>& Out)
{
    static const TSet<FName> Heavy = {TEXT("GetAllActorsOfClass"), TEXT("GetAllActorsWithTag"), TEXT("GetAllActorsWithInterface"),
        TEXT("GetAllActorsOfClassWithTag"), TEXT("GetActorOfClass"), TEXT("GetComponentByClass"), TEXT("K2_GetComponentsByClass"),
        TEXT("GetComponentsByTag")};
    static const FName TickEvent(TEXT("ReceiveTick"));
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
        if (!Event || Event->EventReference.GetMemberName() != TickEvent)
        {
            continue;
        }
        TSet<const UEdGraphNode*> Runs = Flood({Event});
        // One step back along the data wires: a pure search feeding what Tick runs runs every frame too.
        for (const UEdGraphNode* Ran : Runs.Array())
        {
            for (const UEdGraphPin* Pin : Ran->Pins)
            {
                if (IsExec(Pin) || Pin->Direction != EGPD_Input)
                {
                    continue;
                }
                for (const UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    Runs.Add(Linked->GetOwningNode());
                }
            }
        }
        for (const UEdGraphNode* Ran : Runs)
        {
            if (Heavy.Contains(FunctionName(Ran)))
            {
                Out.Add({TEXT("tick_heavy"), TEXT("warning"), Ran, FString::Printf(TEXT("Event Tick runs %s every frame, which searches the level; find it once (BeginPlay) and keep it in a variable."), *FunctionName(Ran).ToString())});
            }
            else if (Cast<UK2Node_SpawnActorFromClass>(Ran))
            {
                Out.Add({TEXT("tick_heavy"), TEXT("warning"), Ran, TEXT("Event Tick spawns an actor every frame; spawn on an event, or check a condition first.")});
            }
        }
    }
}

// Nodes with an exec input that no entry (an event, an input event, a function entry: exec out, none in) reaches. Only
// the first node of each dead chain is named: one with no wire into it (a node wired from a reached one is reached).
// ponytail: a dead loop with no such first node is not named; name its lowest node if that ever matters.
void Unreachable(const UEdGraph* Graph, TArray<FFinding>& Out)
{
    TArray<const UEdGraphNode*> Entries;
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && HasExec(Node, EGPD_Output) && !HasExec(Node, EGPD_Input))
        {
            Entries.Add(Node);
        }
    }
    const TSet<const UEdGraphNode*> Reached = Flood(Entries);
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node || Reached.Contains(Node) || !HasExec(Node, EGPD_Input))
        {
            continue;
        }
        const bool bHasCaller = Node->Pins.ContainsByPredicate([](const UEdGraphPin* Pin)
        {
            return IsExec(Pin) && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0;
        });
        if (!bHasCaller)
        {
            Out.Add({TEXT("unreachable"), TEXT("warning"), Node, TEXT("No event or function leads here: this node and what follows it never run.")});
        }
    }
}

void RandomFanout(const UEdGraph* Graph, TArray<FFinding>& Out)
{
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        if (!Call || !Call->IsNodePure() || !FunctionName(Call).ToString().StartsWith(TEXT("Random")))
        {
            continue;
        }
        TSet<const UEdGraphNode*> Readers;
        for (const UEdGraphPin* Pin : Call->Pins)
        {
            if (Pin->Direction != EGPD_Output)
            {
                continue;
            }
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                Readers.Add(Linked->GetOwningNode());
            }
        }
        if (Readers.Num() > 1)
        {
            Out.Add({TEXT("random_fanout"), TEXT("warning"), Call, FString::Printf(TEXT("%s is pure: each of the %d nodes reading it rolls its own value. Set the roll in a variable once and read that."), *FunctionName(Call).ToString(), Readers.Num())});
        }
    }
}

void InfiniteWhile(const UEdGraph* Graph, TArray<FFinding>& Out)
{
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node);
        const UEdGraphPin* Condition = Macro && Macro->GetMacroGraph() && Macro->GetMacroGraph()->GetName() == TEXT("WhileLoop")
            ? Macro->FindPin(TEXT("Condition"), EGPD_Input) : nullptr;
        if (Condition && Condition->LinkedTo.Num() == 0 && Condition->DefaultValue.Equals(TEXT("true"), ESearchCase::IgnoreCase))
        {
            Out.Add({TEXT("infinite_while"), TEXT("error"), Macro, TEXT("This While Loop's Condition is always true: it never ends, and Play stops it as an infinite loop.")});
        }
    }
}

// Object variables read but never set, not editable per placed actor and not exposed on spawn, with no default.
void NeverAssigned(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FFinding>& Out)
{
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        const FName Category = Variable.VarType.PinCategory;
        const bool bObject = Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Interface;
        if (!bObject || Variable.VarType.IsContainer() || !Variable.DefaultValue.IsEmpty() ||
            !(Variable.PropertyFlags & CPF_DisableEditOnInstance) || (Variable.PropertyFlags & CPF_ExposeOnSpawn))
        {
            continue;
        }
        const UEdGraphNode* FirstRead = nullptr;
        bool bSet = false;
        for (const UEdGraph* Graph : Graphs)
        {
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                const UK2Node_Variable* Access = Cast<UK2Node_Variable>(Node);
                if (!Access || Access->GetVarName() != Variable.VarName || !Access->VariableReference.IsSelfContext())
                {
                    continue;
                }
                bSet |= Access->IsA<UK2Node_VariableSet>();
                if (!FirstRead && Access->IsA<UK2Node_VariableGet>())
                {
                    FirstRead = Access;
                }
            }
        }
        if (FirstRead && !bSet)
        {
            Out.Add({TEXT("never_assigned"), TEXT("warning"), FirstRead, FString::Printf(TEXT("%s is read but nothing assigns it, it is not Instance Editable nor Expose on Spawn, and it has no default: every read is None (Accessed None in Play)."), *Variable.VarName.ToString())});
        }
    }
}
} // namespace McpBPLint
