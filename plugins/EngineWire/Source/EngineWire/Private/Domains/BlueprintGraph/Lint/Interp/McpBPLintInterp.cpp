// Each entry of a graph run to a fixpoint over its exec flow (see McpBPLintRunner.h): where paths meet their states are
// joined, and a state still moving at a node after a few passes is widened, so loops settle. A budget of node visits
// per entry bounds the work; an entry past it is left out rather than judged on states that have not settled.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintInterp.h"

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintRunner.h"
#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_Knot.h"

namespace McpBPLint
{
namespace
{
constexpr int32 MaxRunVisits = 20000;
constexpr int32 MaxRunFrames = 32;
constexpr int32 MaxRunStacks = 4096;
constexpr int32 WidenAfterUpdates = 3;
constexpr int32 MaxExecKnots = 64;

// Where a chain starts: a node with exec outputs and no exec input (an event, a function's entry, a collapsed graph's).
bool IsChainStart(const UEdGraphNode* Node)
{
    bool bOutput = false;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (IsExecPin(Pin) && Pin->Direction == EGPD_Input)
        {
            return false;
        }
        bOutput |= IsExecPin(Pin);
    }
    return bOutput;
}
} // namespace

bool FRunner::Run(const UEdGraphNode* Entry, const FState& Start, FAnalysis& Out)
{
    Reach(FRunKey{Entry, 0, ERunStep::Run, 0}, Start);
    for (int32 Head = 0; Head < Work.Num() && !bOver; ++Head)
    {
        Vertices[Work[Head]].bQueued = false;
        bOver |= ++Visits > MaxRunVisits;
        Step(Work[Head]);
    }
    if (bOver)
    {
        return false;
    }
    for (const FRunVertex& Vertex : Vertices)
    {
        if (Vertex.Key.Step == ERunStep::Run && Vertex.In.bLive)
        {
            Out.Runs.Add(FRun{Vertex.Key.Node, Vertex.In});
        }
    }
    for (const TPair<const UEdGraphNode*, FState>& Return : Returns)
    {
        Out.LoopReturns.FindOrAdd(Return.Key).Add(Return.Value);
    }
    return true;
}

int32 FRunner::Intern(TArray<FRunFrame>&& Frames)
{
    const int32 Found = Stacks.IndexOfByKey(Frames);
    if (Found != INDEX_NONE)
    {
        return Found;
    }
    bOver |= Stacks.Num() >= MaxRunStacks;
    return Stacks.Add(MoveTemp(Frames));
}

int32 FRunner::Push(int32 Stack, const FRunFrame& Frame)
{
    TArray<FRunFrame> Frames = Stacks[Stack];
    bOver |= Frames.Num() >= MaxRunFrames;
    Frames.Add(Frame);
    return Intern(MoveTemp(Frames));
}

void FRunner::Reach(const FRunKey& Key, const FState& State)
{
    if (!Key.Node || !State.bLive || bOver)
    {
        return;
    }
    const int32* Found = Index.Find(Key);
    const int32 At = Found ? *Found : Vertices.Add(FRunVertex{Key});
    Index.Add(Key, At);
    FRunVertex& Vertex = Vertices[At];
    const FState Merged = Vertex.Updates >= WidenAfterUpdates ? WidenStates(Vertex.In, State) : JoinStates(Vertex.In, State);
    if (Vertex.In.bLive && SameState(Merged, Vertex.In))
    {
        return;
    }
    Vertex.In = Merged;
    ++Vertex.Updates;
    if (!Vertex.bQueued)
    {
        Vertex.bQueued = true;
        Work.Add(At);
    }
}

// A chain ended: back to the Sequence or loop it ran in, or the entry is done.
void FRunner::Return(int32 Stack, const FState& State)
{
    if (Stacks[Stack].Num() == 0)
    {
        return;
    }
    TArray<FRunFrame> Below = Stacks[Stack];
    const FRunFrame Frame = Below.Pop();
    const int32 Outer = Intern(MoveTemp(Below));
    if (Frame.Next == INDEX_NONE)
    {
        FState& Back = Returns.FindOrAdd(Frame.Node);
        Back = JoinStates(Back, State);
        Reach(FRunKey{Frame.Node, Outer, ERunStep::Run, 0}, State);
    }
    else
    {
        Reach(FRunKey{Frame.Node, Outer, ERunStep::Resume, Frame.Next}, State);
    }
}

void FRunner::Follow(const UEdGraphPin* Exit, const FState& State, int32 Stack, int32 Hops)
{
    if (!Exit || Exit->LinkedTo.Num() == 0 || Hops > MaxExecKnots)
    {
        Return(Stack, State);
        return;
    }
    for (const UEdGraphPin* Linked : Exit->LinkedTo)
    {
        const UEdGraphNode* Next = Linked ? Linked->GetOwningNode() : nullptr;
        if (const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Next))
        {
            Follow(Knot->GetOutputPin(), State, Stack, Hops + 1);
        }
        else if (Next && Linked->PinName == TEXT("Break") && !MacroName(Next).IsNone())
        {
            Break(Next, Stack, State);
        }
        else
        {
            Reach(FRunKey{Next, Stack, ERunStep::Run, 0}, State);
        }
    }
}

// A loop's Break: its Completed runs in the stack the loop itself ran in.
void FRunner::Break(const UEdGraphNode* Loop, int32 Stack, const FState& State)
{
    TArray<FRunFrame> Frames = Stacks[Stack];
    const int32 At = Frames.FindLastByPredicate([Loop](const FRunFrame& Frame) { return Frame.Node == Loop && Frame.Next == INDEX_NONE; });
    Frames.SetNum(At == INDEX_NONE ? Frames.Num() : At);
    Reach(FRunKey{Loop, Intern(MoveTemp(Frames)), ERunStep::Exit, 0}, State);
}

FAnalysis Interpret(const UEdGraph* Graph, const FMembers& Members)
{
    FAnalysis Analysis;
    FState Start = Members.Entry;
    Start.bLive = true;
    AddLocals(Graph, Start);
    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node && IsChainStart(Node))
        {
            FRunner Runner(Members);
            Analysis.Skipped += Runner.Run(Node, Start, Analysis) ? 0 : 1;
        }
    }
    return Analysis;
}
} // namespace McpBPLint
