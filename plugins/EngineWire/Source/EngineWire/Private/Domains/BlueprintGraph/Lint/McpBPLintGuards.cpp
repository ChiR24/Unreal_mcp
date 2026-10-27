// unguarded_read: an object variable set from something that can give None (a cast, a call, a Set with nothing wired in)
// and used as the target of a call or a property where not every exec path to that use passes a validity check of it:
// the Is Valid macro, a Branch on Is Valid, a validated get or an impure cast of the variable (McpBPLintFlow.cpp).

#include "Domains/BlueprintGraph/Lint/McpBPLintRules.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Knot.h"
#include "K2Node_Message.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Misc/Optional.h"

namespace McpBPLint
{
namespace
{
constexpr int32 MaxPureHops = 64;

// The exec nodes where the value on Output is used as a target: a node whose self pin it feeds, or, when that node is
// pure, every exec node its results reach. An interface message to None does nothing, so it is no use.
void TargetUses(const UEdGraphPin* Output, TArray<const UEdGraphNode*>& OutUses)
{
    // Each open output, and whether every reader of it uses the variable (the output of a pure node that took the
    // variable as its target) or only a self pin does (the variable itself).
    TArray<TPair<const UEdGraphPin*, bool>> Open;
    Open.Emplace(Output, false);
    TSet<const UEdGraphNode*> Seen;
    for (int32 Hop = 0; Open.Num() > 0 && Hop < MaxPureHops; ++Hop)
    {
        const TPair<const UEdGraphPin*, bool> From = Open.Pop();
        for (const UEdGraphPin* Linked : From.Key->LinkedTo)
        {
            const UEdGraphNode* Node = Linked ? Linked->GetOwningNode() : nullptr;
            if (const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Node))
            {
                Open.Emplace(Knot->GetOutputPin(), From.Value);
                continue;
            }
            if (!Node || !(From.Value || Linked->PinName == UEdGraphSchema_K2::PN_Self) || Node->IsA<UK2Node_Message>() ||
                Seen.Contains(Node))
            {
                continue;
            }
            Seen.Add(Node);
            if (HasExecPin(Node))
            {
                OutUses.Add(Node);
                continue;
            }
            for (const UEdGraphPin* Result : Node->Pins)
            {
                if (Result->Direction == EGPD_Output && !IsExecPin(Result))
                {
                    Open.Emplace(Result, true);
                }
            }
        }
    }
}

// What a Set of the variable can give it that is None: an unwired value, a cast's result, a call's result.
FString NoneSource(const UK2Node_VariableSet* Set)
{
    const UEdGraphPin* Value = Set->FindPin(Set->GetVarName(), EGPD_Input);
    const UEdGraphPin* Source = SourceOf(Value);
    const UEdGraphNode* From = Source ? Source->GetOwningNode() : nullptr;
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(From);
    if (Value && Value->LinkedTo.Num() == 0 && !Value->DefaultObject)
    {
        return TEXT("a Set with nothing wired in (which sets None)");
    }
    if (Cast<UK2Node_DynamicCast>(From))
    {
        return TEXT("a cast (None when the cast fails)");
    }
    return Call && Call->GetTargetFunction()
        ? FString::Printf(TEXT("%s (which can return None)"), *Call->GetTargetFunction()->GetName()) : FString();
}
} // namespace

void UnguardedRead(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FFinding>& Out)
{
    TSet<FName> Objects;
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        const FName Category = Variable.VarType.PinCategory;
        if ((Category == UEdGraphSchema_K2::PC_Object || Category == UEdGraphSchema_K2::PC_Interface) && !Variable.VarType.IsContainer())
        {
            Objects.Add(Variable.VarName);
        }
    }
    TMap<FName, FString> Nullable; // variable -> what can make it None
    for (const UEdGraph* Graph : Graphs)
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node);
            const FName Variable = Set && Set->VariableReference.IsSelfContext() ? Set->GetVarName() : NAME_None;
            const FString Source = Objects.Contains(Variable) && !Nullable.Contains(Variable) ? NoneSource(Set) : FString();
            if (!Source.IsEmpty())
            {
                Nullable.Add(Variable, Source);
            }
        }
    }
    TMap<FName, TPair<const UEdGraphNode*, int32>> Unguarded; // variable -> its first unguarded use, and how many
    for (const UEdGraph* Graph : Graphs)
    {
        TOptional<FFlow> Flow;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node);
            const FName Variable = Get && Get->VariableReference.IsSelfContext() ? Get->GetVarName() : NAME_None;
            const UEdGraphPin* Output = Nullable.Contains(Variable) ? Get->FindPin(Variable, EGPD_Output) : nullptr;
            TArray<const UEdGraphNode*> Uses;
            if (Output)
            {
                TargetUses(Output, Uses);
            }
            for (const UEdGraphNode* Use : Uses)
            {
                if (!Flow.IsSet())
                {
                    Flow = BuildFlow(Graph);
                }
                const int32* At = Flow->Vertex.Find(Use);
                if (At && Flow->Idom[*At] != INDEX_NONE && !Guarded(*Flow, *At, Variable))
                {
                    ++Unguarded.FindOrAdd(Variable, TPair<const UEdGraphNode*, int32>(Use, 0)).Value;
                }
            }
        }
    }
    for (const TPair<FName, TPair<const UEdGraphNode*, int32>>& Pair : Unguarded)
    {
        const FString& Source = Nullable[Pair.Key];
        Out.Add({TEXT("unguarded_read"), TEXT("warning"), Pair.Value.Key, FString::Printf(TEXT("%s is set from %s, and this node uses it as a target with no validity check on every path here (%d use(s) like this): when it is None, Play logs Accessed None. Check it first (an Is Valid node, or a validated get)%s."), *Pair.Key.ToString(), *Source, Pair.Value.Value, Source.StartsWith(TEXT("a cast")) ? TEXT(", or act on the cast's Cast Failed exit") : TEXT(""))});
    }
}
} // namespace McpBPLint
