#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"

namespace McpGraphCode
{
namespace Emit
{
FPinRef FDecompiler::FollowExec(const FNode& Node, const TCHAR* PinName) const
{
    const FPin* Pin = Node.Outs.FindByPredicate([PinName](const FPin& Each) { return Each.bExec && SameText(Each.Name, PinName); });
    return Pin && Pin->Links.Num() > 0 ? Pin->Links[0] : FPinRef();
}

// The first node both sides of a branch reach: printed after the (if ...), not inside it.
int32 FDecompiler::FindJoin(int32 Then, int32 Else) const
{
    if (Then == INDEX_NONE || Else == INDEX_NONE)
    {
        return INDEX_NONE;
    }
    const auto Walk = [this](int32 Start, TFunctionRef<bool(int32)> Visit)
    {
        TArray<int32> Queue = {Start};
        TSet<int32> Seen;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const int32 At = Queue[Head];
            if (Seen.Contains(At))
            {
                continue;
            }
            Seen.Add(At);
            if (!Visit(At))
            {
                return At;
            }
            for (const FPin& Pin : Graph.Nodes[At].Outs)
            {
                if (Pin.bExec && Pin.Links.Num() > 0)
                {
                    Queue.Add(Pin.Links[0].Node);
                }
            }
        }
        return int32(INDEX_NONE);
    };
    TSet<int32> FromThen;
    Walk(Then, [&FromThen](int32 At) { FromThen.Add(At); return true; });
    return Walk(Else, [&FromThen](int32 At) { return !FromThen.Contains(At); });
}

void FDecompiler::Stmts(FPinRef At, FState& State, int32 BreakNode, int32 StopNode)
{
    while (At.Node != INDEX_NONE)
    {
        const int32 Index = At.Node;
        const FNode& Node = Graph.Nodes[Index];
        if (Index == StopNode)
        {
            return;
        }
        if (Index == BreakNode && Node.Ins.IsValidIndex(At.Pin) && SameText(Node.Ins[At.Pin].Name, TEXT("Break")))
        {
            State.Lines.Add(FForm{State.Pad() + TEXT("(break"), FString(), {}, 1});
            return;
        }
        if (Visited.Contains(Index))
        {
            Goto(At, State);
            return;
        }
        Visited.Add(Index);
        Placed.Add(Index);
        bool bIndexRead = false;
        bool bOtherExec = false;
        for (int32 Out = 0; Out < Node.Outs.Num(); ++Out)
        {
            const FPin& Pin = Node.Outs[Out];
            bIndexRead |= SameText(Pin.Name, TEXT("Array Index")) && Needed.Contains(FKey(Index, Out));
            bOtherExec |= Pin.bExec && Pin.Links.Num() > 0 && !SameText(Pin.Name, TEXT("then"));
        }
        switch (Node.Kind)
        {
        case EKind::Return:
            ReturnStmt(Node, State);
            return;
        case EKind::Branch:
        {
            const int32 Join = BranchStmt(Index, State, StopNode);
            const int32 JoinIn = Join == INDEX_NONE ? INDEX_NONE : Graph.Nodes[Join].Ins.IndexOfByPredicate([](const FPin& Pin) { return Pin.bExec; });
            At = FPinRef{Join, JoinIn};
            continue;
        }
        case EKind::ForLoop:
        case EKind::ForLoopBreak:
            At = ForStmt(Index, State, Node.Kind == EKind::ForLoopBreak ? Index : INDEX_NONE);
            continue;
        case EKind::ForEach:
        case EKind::ForEachBreak:
            if (!bIndexRead)
            {
                At = ForEachStmt(Index, State, Node.Kind == EKind::ForEachBreak ? Index : INDEX_NONE);
                continue;
            }
            // (for elem array) has no index: the loop prints as a call with its continuations.
            MultiExec(Index, State);
            return;
        case EKind::While:
        {
            const FPin* Condition = FindPinNamed(Node.Ins, TEXT("Condition"));
            const FString Test = Condition ? Expr(*Condition, State.Bindings) : FString(TEXT("true"));
            State.Lines.Add(FForm{State.Pad() + TEXT("(while ") + Test, Node.Id, LoopBody(Node, State, INDEX_NONE), 1});
            At = FollowExec(Node, TEXT("Completed"));
            continue;
        }
        case EKind::Knot:
        {
            const FPin* Out = Node.Outs.FindByPredicate([](const FPin& Pin) { return Pin.bExec && Pin.Links.Num() > 0; });
            At = Out ? Out->Links[0] : FPinRef();
            continue;
        }
        case EKind::Switch:
        {
            const TArray<const FPin*> Ins = DataIns(Node);
            const FString Value = Ins.Num() > 0 ? Expr(*Ins[0], State.Bindings) : FString(TEXT("_"));
            State.Lines.Add(FForm{FString::Printf(TEXT("%s(switch %s %s"), *State.Pad(), *Node.TypeId, *Value), Node.Id, Continuations(Index, State), 1});
            return;
        }
        default:
            break;
        }
        if (bOtherExec)
        {
            MultiExec(Index, State);
            return;
        }
        At = CallStmt(Index, State);
    }
}

// The path runs on into a node already printed: Epic's reader stops here and the rest goes unsaid.
void FDecompiler::Goto(FPinRef At, FState& State)
{
    TSet<int32> Seen;
    while (Graph.Nodes[At.Node].Kind == EKind::Knot && !Seen.Contains(At.Node))
    {
        Seen.Add(At.Node);
        const FPin* Out = Graph.Nodes[At.Node].Outs.FindByPredicate([](const FPin& Pin) { return Pin.bExec && Pin.Links.Num() > 0; });
        if (!Out)
        {
            return;
        }
        At = Out->Links[0];
    }
    const FNode& Target = Graph.Nodes[At.Node];
    FString Line = State.Pad() + TEXT("(goto ") + Target.Id;
    // Which input, when the node has more than one (a loop's Break, a gate's Open).
    if (Target.Ins.FilterByPredicate([](const FPin& Pin) { return Pin.bExec; }).Num() > 1 && Target.Ins.IsValidIndex(At.Pin))
    {
        Line += TEXT(" ") + FormatPinName(Target.Ins[At.Pin].Name);
    }
    State.Lines.Add(FForm{Line, FString(), {}, 1});
}

int32 FDecompiler::BranchStmt(int32 Index, FState& State, int32 StopNode)
{
    const FNode& Node = Graph.Nodes[Index];
    const FPin* Condition = FindPinNamed(Node.Ins, TEXT("Condition"));
    const FString Test = Condition ? Expr(*Condition, State.Bindings) : FString(TEXT("true"));
    const FPinRef Then = FollowExec(Node, TEXT("then"));
    const FPinRef Else = FollowExec(Node, TEXT("else"));
    const int32 Join = FindJoin(Then.Node, Else.Node);
    const int32 InnerStop = Join != INDEX_NONE ? Join : StopNode;
    FState ThenState = State.Child();
    if (Then.Node != INDEX_NONE)
    {
        Stmts(Then, ThenState, INDEX_NONE, InnerStop);
    }
    TArray<FForm> Body = OrPlaceholder(ThenState.Lines, ThenState);
    const bool bElse = Else.Node != INDEX_NONE && Else.Node != Join;
    if (bElse && Graph.Nodes[Else.Node].Kind == EKind::Branch && !Visited.Contains(Else.Node))
    {
        Body.Add(ElifForm(Else.Node, State.Child(1), InnerStop));
    }
    else if (bElse)
    {
        if (ThenState.Lines.Num() == 0)
        {
            // Nothing when true: the condition is turned so the false side is the body.
            FState Flipped = State.Child();
            Stmts(Else, Flipped, INDEX_NONE, InnerStop);
            if (Flipped.Lines.Num() > 0)
            {
                State.Lines.Add(FForm{State.Pad() + TEXT("(if ") + InvertExpr(Test), Node.Id, MoveTemp(Flipped.Lines), 1});
                return Join;
            }
        }
        FState ElseState = State.Child(2);
        Stmts(Else, ElseState, INDEX_NONE, InnerStop);
        Body.Add(FForm{State.Pad() + TEXT("  (else"), FString(), OrPlaceholder(ElseState.Lines, ElseState), 1});
    }
    State.Lines.Add(FForm{State.Pad() + TEXT("(if ") + Test, Node.Id, MoveTemp(Body), 1});
    return Join;
}

FForm FDecompiler::ElifForm(int32 Index, const FState& State, int32 StopNode)
{
    Visited.Add(Index);
    Placed.Add(Index);
    const FNode& Node = Graph.Nodes[Index];
    const FPin* Condition = FindPinNamed(Node.Ins, TEXT("Condition"));
    const FString Test = Condition ? Expr(*Condition, State.Bindings) : FString(TEXT("true"));
    const FPinRef Then = FollowExec(Node, TEXT("then"));
    const FPinRef Else = FollowExec(Node, TEXT("else"));
    FState ThenState = State.Child();
    if (Then.Node != INDEX_NONE)
    {
        Stmts(Then, ThenState, INDEX_NONE, StopNode);
    }
    TArray<FForm> Body = OrPlaceholder(ThenState.Lines, ThenState);
    const bool bElse = Else.Node != INDEX_NONE && Else.Node != StopNode;
    if (bElse && Graph.Nodes[Else.Node].Kind == EKind::Branch && !Visited.Contains(Else.Node))
    {
        Body.Add(ElifForm(Else.Node, State.Child(1), StopNode));
    }
    else if (bElse)
    {
        FState ElseState = State.Child(2);
        Stmts(Else, ElseState, INDEX_NONE, StopNode);
        Body.Add(FForm{State.Pad() + TEXT("  (else"), FString(), OrPlaceholder(ElseState.Lines, ElseState), 1});
    }
    return FForm{State.Pad() + TEXT("(elif ") + Test, Node.Id, MoveTemp(Body), 1};
}

} // namespace Emit
} // namespace McpGraphCode
