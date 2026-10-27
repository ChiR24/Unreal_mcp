// A graph's exec flow and its immediate dominators, by the iteration of Cooper, Harvey and Kennedy ("A Simple, Fast
// Dominance Algorithm"): a node dominates another when every path from an entry to the second passes through it.

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"

namespace McpBPLint
{
namespace
{
constexpr int32 MaxKnotHops = 64;

// The member variable an input reads straight from a getter of it (pure or validated), else NAME_None.
FName GotVariable(const UEdGraphPin* Input)
{
    const UEdGraphPin* Source = SourceOf(Input);
    const UK2Node_VariableGet* Get = Source ? Cast<UK2Node_VariableGet>(Source->GetOwningNode()) : nullptr;
    return Get && Get->VariableReference.IsSelfContext() ? Get->GetVarName() : NAME_None;
}

// The first data input of Node that reads a member variable straight from a getter, the self pin left out.
FName FirstGotVariable(const UEdGraphNode* Node)
{
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (!IsExecPin(Pin) && Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Self && !GotVariable(Pin).IsNone())
        {
            return GotVariable(Pin);
        }
    }
    return NAME_None;
}

// The variable Node checks, and the exit past which it holds an object, when Node is such a check.
FName CheckedVariable(const UEdGraphNode* Node, const UEdGraphPin*& OutValidExit)
{
    static const FName IsValidFunction(TEXT("IsValid"));
    OutValidExit = nullptr;
    FName Variable = NAME_None;
    if (const UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        OutValidExit = CastNode->IsNodePure() ? nullptr : CastNode->GetValidCastPin();
        Variable = GotVariable(CastNode->GetCastSourcePin());
    }
    else if (const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
    {
        // A validated get of an object (a Boolean's validated get branches on its value instead).
        const UEdGraphPin* Value = Get->IsNodePure() ? nullptr : Get->FindPin(Get->GetVarName(), EGPD_Output);
        if (Value && Value->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Get->VariableReference.IsSelfContext())
        {
            OutValidExit = Get->FindPin(UEdGraphSchema_K2::PN_Then);
            Variable = Get->GetVarName();
        }
    }
    else if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(Node))
    {
        // The standard Is Valid macro: exits Is Valid and Is Not Valid, its object the one data input.
        if (Macro->GetMacroGraph() && Macro->GetMacroGraph()->GetName() == TEXT("IsValid"))
        {
            UEdGraphPin* const* Exit = Macro->Pins.FindByPredicate([](const UEdGraphPin* Pin)
            {
                return IsExecPin(Pin) && Pin->Direction == EGPD_Output && !Pin->PinName.ToString().Contains(TEXT("Not"));
            });
            OutValidExit = Exit ? *Exit : nullptr;
            Variable = FirstGotVariable(Macro);
        }
    }
    else if (const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Node))
    {
        const UEdGraphPin* Condition = SourceOf(Branch->GetConditionPin());
        const UK2Node_CallFunction* Test = Condition ? Cast<UK2Node_CallFunction>(Condition->GetOwningNode()) : nullptr;
        if (Test && Test->GetTargetFunction() && Test->GetTargetFunction()->GetFName() == IsValidFunction)
        {
            OutValidExit = Branch->FindPin(UEdGraphSchema_K2::PN_Then);
            Variable = FirstGotVariable(Test);
        }
    }
    return OutValidExit ? Variable : NAME_None;
}

void Dominate(FFlow& Flow)
{
    const int32 Count = Flow.Next.Num();
    TArray<int32> Order, Post, Visited;
    Order.Init(INDEX_NONE, Count);
    Visited.Init(0, Count);
    TArray<TPair<int32, int32>> Stack; // vertex, next edge to follow
    Stack.Emplace(0, 0);
    Visited[0] = 1;
    while (Stack.Num() > 0)
    {
        const int32 At = Stack.Last().Key;
        const int32 Edge = Stack.Last().Value++;
        if (Edge >= Flow.Next[At].Num())
        {
            Order[At] = Post.Add(At);
            Stack.Pop();
        }
        else if (!Visited[Flow.Next[At][Edge]])
        {
            Visited[Flow.Next[At][Edge]] = 1;
            Stack.Emplace(Flow.Next[At][Edge], 0);
        }
    }
    TArray<TArray<int32>> Prev;
    Prev.SetNum(Count);
    for (int32 From = 0; From < Count; ++From)
    {
        for (const int32 To : Flow.Next[From])
        {
            Prev[To].Add(From);
        }
    }
    Flow.Idom.Init(INDEX_NONE, Count);
    Flow.Idom[0] = 0;
    for (bool bChanged = true; bChanged;)
    {
        bChanged = false;
        for (int32 Index = Post.Num() - 2; Index >= 0; --Index) // reverse postorder; the root finishes last
        {
            const int32 At = Post[Index];
            int32 New = INDEX_NONE;
            for (int32 Up : Prev[At])
            {
                if (Flow.Idom[Up] == INDEX_NONE)
                {
                    continue; // not processed yet, or no entry reaches it
                }
                for (int32 Other = New; Other != INDEX_NONE && Up != Other;)
                {
                    while (Order[Up] < Order[Other])
                    {
                        Up = Flow.Idom[Up];
                    }
                    while (Order[Other] < Order[Up])
                    {
                        Other = Flow.Idom[Other];
                    }
                }
                New = Up;
            }
            bChanged |= New != Flow.Idom[At];
            Flow.Idom[At] = New;
        }
    }
}
} // namespace

bool IsExecPin(const UEdGraphPin* Pin)
{
    return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

bool HasExecPin(const UEdGraphNode* Node)
{
    return Node->Pins.ContainsByPredicate([](const UEdGraphPin* Pin) { return IsExecPin(Pin); });
}

const UEdGraphPin* SourceOf(const UEdGraphPin* Input)
{
    const UEdGraphPin* Source = Input && Input->LinkedTo.Num() == 1 ? Input->LinkedTo[0] : nullptr;
    for (int32 Hop = 0; Source && Hop < MaxKnotHops; ++Hop)
    {
        const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Source->GetOwningNode());
        if (!Knot)
        {
            return Source;
        }
        const UEdGraphPin* In = Knot->GetInputPin();
        Source = In && In->LinkedTo.Num() == 1 ? In->LinkedTo[0] : nullptr;
    }
    return nullptr;
}

FFlow BuildFlow(const UEdGraph* Graph)
{
    FFlow Flow;
    Flow.Vouches.Add(NAME_None);
    Flow.Next.AddDefaulted();
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && HasExecPin(Node))
        {
            Flow.Vertex.Add(Node, Flow.Vouches.Add(NAME_None));
            Flow.Next.AddDefaulted();
        }
    }
    for (const TPair<const UEdGraphNode*, int32>& Pair : Flow.Vertex)
    {
        const UEdGraphPin* ValidExit = nullptr;
        const FName Checked = CheckedVariable(Pair.Key, ValidExit);
        const int32 Past = Checked.IsNone() ? INDEX_NONE : Flow.Vouches.Add(Checked);
        if (Past != INDEX_NONE)
        {
            Flow.Next.AddDefaulted();
            Flow.Next[Pair.Value].Add(Past);
        }
        bool bEntry = true;
        for (const UEdGraphPin* Pin : Pair.Key->Pins)
        {
            if (!IsExecPin(Pin))
            {
                continue;
            }
            bEntry &= Pin->Direction != EGPD_Input;
            const int32 From = Pin == ValidExit && Past != INDEX_NONE ? Past : Pair.Value;
            for (const UEdGraphPin* Linked : Pin->LinkedTo)
            {
                const int32* To = Linked && Pin->Direction == EGPD_Output ? Flow.Vertex.Find(Linked->GetOwningNode()) : nullptr;
                if (To)
                {
                    Flow.Next[From].Add(*To);
                }
            }
        }
        if (bEntry)
        {
            Flow.Next[0].Add(Pair.Value); // an event, a function entry, a collapsed graph's tunnel
        }
    }
    Dominate(Flow);
    return Flow;
}

bool Guarded(const FFlow& Flow, int32 At, FName Variable)
{
    for (int32 Up = Flow.Idom[At]; Up > 0; Up = Flow.Idom[Up])
    {
        if (Flow.Vouches[Up] == Variable)
        {
            return true;
        }
    }
    return false;
}
} // namespace McpBPLint
