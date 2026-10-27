#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"

namespace McpGraphCode
{
namespace Emit
{
namespace LoopForms
{
FString Lhs(const TArray<FString>& Names)
{
    return Names.Num() > 1 ? TEXT("(") + FString::Join(Names, TEXT(" ")) + TEXT(")") : Names[0];
}
} // namespace LoopForms

using namespace LoopForms;

FPinRef FDecompiler::ForStmt(int32 Index, FState& State, int32 BreakNode)
{
    const FNode& Node = Graph.Nodes[Index];
    const FPin* First = FindPinNamed(Node.Ins, TEXT("FirstIndex"));
    const FPin* Last = FindPinNamed(Node.Ins, TEXT("LastIndex"));
    const FString FirstExpr = First ? Expr(*First, State.Bindings) : FString(TEXT("0"));
    const FString LastExpr = Last ? Expr(*Last, State.Bindings) : FString(TEXT("0"));
    FString StopExpr = Last ? UnwrapDecrement(*Last, State.Bindings) : FString();
    StopExpr = StopExpr.IsEmpty() ? IncrementExpr(LastExpr) : StopExpr;
    FString Var = TEXT("_");
    const int32 Out = Node.Outs.IndexOfByPredicate([](const FPin& Pin) { return SameText(Pin.Name, TEXT("Index")); });
    if (Out != INDEX_NONE && Needed.Contains(FKey(Index, Out)))
    {
        Var = AllocName(Node.Outs[Out].Name, State.Bindings);
        State.Bindings.Add(FKey(Index, Out), Var);
        Named.Add(FKey(Index, Out), Var);
    }
    const FString Range = FirstExpr == TEXT("0") ? FString::Printf(TEXT("(range %s)"), *StopExpr)
                                                 : FString::Printf(TEXT("(range %s %s)"), *FirstExpr, *StopExpr);
    State.Lines.Add(FForm{FString::Printf(TEXT("%s(for %s %s"), *State.Pad(), *Var, *Range), Node.Id, LoopBody(Node, State, BreakNode), 1});
    return FollowExec(Node, TEXT("Completed"));
}

FPinRef FDecompiler::ForEachStmt(int32 Index, FState& State, int32 BreakNode)
{
    const FNode& Node = Graph.Nodes[Index];
    const FPin* Array = FindPinNamed(Node.Ins, TEXT("Array"));
    const FString ArrayExpr = Array ? Expr(*Array, State.Bindings) : FString(TEXT("_"));
    FString Var = TEXT("_");
    const int32 Out = Node.Outs.IndexOfByPredicate([](const FPin& Pin) { return SameText(Pin.Name, TEXT("Array Element")); });
    if (Out != INDEX_NONE && Needed.Contains(FKey(Index, Out)))
    {
        Var = AllocName(Node.Outs[Out].Name, State.Bindings);
        State.Bindings.Add(FKey(Index, Out), Var);
        Named.Add(FKey(Index, Out), Var);
    }
    State.Lines.Add(FForm{FString::Printf(TEXT("%s(for %s %s"), *State.Pad(), *Var, *ArrayExpr), Node.Id, LoopBody(Node, State, BreakNode), 1});
    return FollowExec(Node, TEXT("Completed"));
}

TArray<FForm> FDecompiler::LoopBody(const FNode& Node, const FState& State, int32 BreakNode)
{
    FState Body = State.Child();
    const FPinRef First = FollowExec(Node, TEXT("LoopBody"));
    if (First.Node != INDEX_NONE)
    {
        Stmts(First, Body, BreakNode);
    }
    return OrPlaceholder(MoveTemp(Body.Lines), Body);
}

// One (:Pin ...) per wired exec output, the pin name quoted when it holds a space.
TArray<FForm> FDecompiler::Continuations(int32 Index, const FState& State)
{
    const FString Pad = FString::ChrN((State.Indent + 1) * 2, TEXT(' '));
    TArray<FForm> Forms;
    for (const FPin& Pin : Graph.Nodes[Index].Outs)
    {
        if (Pin.bExec && Pin.Links.Num() > 0)
        {
            FState Branch = State.Child(2);
            Stmts(Pin.Links[0], Branch);
            Forms.Add(FForm{Pad + TEXT("(:") + FormatPinName(Pin.Name), FString(), MoveTemp(Branch.Lines), 1});
        }
    }
    return Forms;
}

void FDecompiler::MultiExec(int32 Index, FState& State)
{
    const FString Inner = CallInner(Index, State.Bindings);
    const TArray<FString> Names = Targets(Index, State);
    TArray<FForm> Branches = Continuations(Index, State);
    const FString Head = Names.Num() > 0 ? FString::Printf(TEXT("%s(bind %s (%s)"), *State.Pad(), *Lhs(Names), *Inner) : State.Pad() + TEXT("(") + Inner;
    State.Lines.Add(FForm{Head, Graph.Nodes[Index].Id, MoveTemp(Branches), 1});
}

FPinRef FDecompiler::CallStmt(int32 Index, FState& State)
{
    const FString Inner = CallInner(Index, State.Bindings);
    const TArray<FString> Names = Targets(Index, State);
    State.Lines.Add(Names.Num() > 0 ? FForm{FString::Printf(TEXT("%s(bind %s (%s"), *State.Pad(), *Lhs(Names), *Inner), Graph.Nodes[Index].Id, {}, 2}
                                    : FForm{State.Pad() + TEXT("(") + Inner, Graph.Nodes[Index].Id, {}, 1});
    return FollowExec(Graph.Nodes[Index], TEXT("then"));
}

void FDecompiler::ReturnStmt(const FNode& Node, FState& State)
{
    TArray<FString> Values;
    for (const FPin* In : DataIns(Node))
    {
        Values.Add(Expr(*In, State.Bindings));
    }
    const FString Head = Values.Num() > 0 ? TEXT("(return ") + FString::Join(Values, TEXT(" ")) : FString(TEXT("(return"));
    State.Lines.Add(FForm{State.Pad() + Head, Node.Id, {}, 1});
}
} // namespace Emit
} // namespace McpGraphCode
