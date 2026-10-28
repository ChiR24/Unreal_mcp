// One step of the fixpoint (McpBPLintRunner.h): a node runs on its state and passes it on along the exits that can fire,
// a Branch narrowed by its condition on each, a Sequence through its outputs in turn, a loop into its body (with its
// index, or its condition holding) and out through Completed.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintRunner.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"

namespace McpBPLint
{
namespace
{
bool IsLoopMacro(const UEdGraphNode* Node)
{
    const FString Macro = MacroName(Node).ToString();
    return Macro == TEXT("WhileLoop") || Macro.Contains(TEXT("ForLoop")) || Macro.Contains(TEXT("ForEachLoop"));
}

// A node that waits (Delay, an async task): its chain hands control back at once and goes on later.
bool IsLatentNode(const UEdGraphNode* Node)
{
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
    const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
    return (Function && Function->HasMetaData(FBlueprintMetadata::MD_Latent)) || Node->IsA<UK2Node_BaseAsyncTask>();
}

TArray<const UEdGraphPin*> ExecOutputsOf(const UEdGraphNode* Node)
{
    TArray<const UEdGraphPin*> Outputs;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (IsExecPin(Pin) && Pin->Direction == EGPD_Output)
        {
            Outputs.Add(Pin);
        }
    }
    return Outputs;
}

// A loop body's index: [First, Last] for a ForLoop, [0, Length - 1] over an array; false when the body never runs.
bool SetLoopIndex(const UEdGraphNode* Loop, const FEval& Eval, FState& Body)
{
    FRange Index;
    if (MacroName(Loop).ToString().Contains(TEXT("ForEachLoop")))
    {
        const FRange Length = Eval.Array(Loop->FindPin(TEXT("Array"), EGPD_Input)).Length;
        Index = Refine(FRange::Between(0.0, Unbounded), ECompare::Less, Length, true);
    }
    else
    {
        const FRange First = Eval.Number(Loop->FindPin(TEXT("FirstIndex"), EGPD_Input));
        const FRange Last = Eval.Number(Loop->FindPin(TEXT("LastIndex"), EGPD_Input));
        Index = Refine(Refine(FRange::Unknown(First.bOpaque || Last.bOpaque), ECompare::GreaterEqual, First, true), ECompare::LessEqual, Last, true);
    }
    Body.Numbers.Add(LoopIndexKey(Loop), Index);
    return !Index.bNone;
}
} // namespace

// A Sequence runs its outputs in turn, from From; each chain comes back to it for the next.
void FRunner::Resume(const UEdGraphNode* Sequence, int32 Stack, int32 From, const FState& State)
{
    const TArray<const UEdGraphPin*> Outputs = ExecOutputsOf(Sequence);
    for (int32 Output = From; Output < Outputs.Num(); ++Output)
    {
        if (Outputs[Output]->LinkedTo.Num() > 0)
        {
            Follow(Outputs[Output], State, Push(Stack, FRunFrame{Sequence, Output + 1}));
            return;
        }
    }
    Return(Stack, State);
}

void FRunner::Loop(const UEdGraphNode* Node, int32 Stack, const FState& State)
{
    const FEval Eval{State};
    FState Body = State;
    FState Done = State;
    bool bBody = true;
    bool bDone = true;
    if (MacroName(Node) == TEXT("WhileLoop"))
    {
        const UEdGraphPin* Condition = Node->FindPin(TEXT("Condition"), EGPD_Input);
        bBody = Eval.Assume(Body, Condition, true);
        bDone = Eval.Assume(Done, Condition, false);
    }
    else
    {
        bBody = SetLoopIndex(Node, Eval, Body);
        Done.Numbers.Remove(LoopIndexKey(Node));
    }
    if (bBody)
    {
        Follow(Node->FindPin(TEXT("LoopBody"), EGPD_Output), Body, Push(Stack, FRunFrame{Node, INDEX_NONE}));
    }
    if (bDone)
    {
        Reach(FRunKey{Node, Stack, ERunStep::Exit, 0}, Done);
    }
}

void FRunner::Step(int32 At)
{
    const FRunKey Key = Vertices[At].Key;
    FState State = Vertices[At].In; // copies: Reach may grow Vertices
    if (Key.Step == ERunStep::Resume)
    {
        Resume(Key.Node, Key.Stack, Key.Output, State);
        return;
    }
    if (Key.Step == ERunStep::Exit)
    {
        Follow(Key.Node->FindPin(TEXT("Completed"), EGPD_Output), State, Key.Stack);
        return;
    }
    if (IsLatentNode(Key.Node))
    {
        // The Sequence or loop it ran in goes on now; what follows the wait runs later, from nothing that encloses it.
        Return(Key.Stack, State);
        Apply(Key.Node, State, Members);
        for (const UEdGraphPin* Output : ExecOutputsOf(Key.Node))
        {
            Follow(Output, State, 0);
        }
        return;
    }
    Apply(Key.Node, State, Members);
    if (Key.Node->IsA<UK2Node_FunctionResult>())
    {
        return; // the function returns
    }
    if (const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Key.Node))
    {
        const FEval Eval{State};
        for (const bool bTrue : {true, false})
        {
            FState Taken = State;
            if (Eval.Assume(Taken, Branch->GetConditionPin(), bTrue))
            {
                Follow(Branch->FindPin(bTrue ? UEdGraphSchema_K2::PN_Then : UEdGraphSchema_K2::PN_Else), Taken, Key.Stack);
            }
        }
        return;
    }
    if (Key.Node->IsA<UK2Node_ExecutionSequence>())
    {
        Resume(Key.Node, Key.Stack, 0, State);
        return;
    }
    if (IsLoopMacro(Key.Node))
    {
        Loop(Key.Node, Key.Stack, State);
        return;
    }
    const TArray<const UEdGraphPin*> Outputs = ExecOutputsOf(Key.Node);
    for (const UEdGraphPin* Output : Outputs)
    {
        Follow(Output, State, Key.Stack);
    }
    if (Outputs.Num() == 0)
    {
        Return(Key.Stack, State);
    }
}
} // namespace McpBPLint
