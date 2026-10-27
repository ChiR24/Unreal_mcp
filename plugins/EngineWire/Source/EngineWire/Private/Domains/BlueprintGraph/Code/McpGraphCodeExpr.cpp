#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"

namespace McpGraphCode
{
namespace Emit
{
FString FDecompiler::Expr(const FPin& Pin, const FBindings& Bindings)
{
    if (Pin.Links.Num() == 0)
    {
        return FormatDefault(Pin);
    }
    bool bHalted = false;
    const FPinRef From = FollowDataKnots(Pin.Links[0], bHalted);
    if (bHalted)
    {
        // A reroute nothing feeds reads its own input's value, as in Epic's reader.
        const TArray<const FPin*> Ins = DataIns(Graph.Nodes[From.Node]);
        FPin Untyped;
        Untyped.Value = Ins.Num() > 0 ? Ins[0]->Value : FString();
        return FormatDefault(Untyped);
    }
    return Source(From.Node, From.Pin, Bindings);
}

FString FDecompiler::Source(int32 NodeIndex, int32 Out, const FBindings& Bindings)
{
    const FKey Key(NodeIndex, Out);
    if (const FString* Bound = Bindings.Find(Key))
    {
        return *Bound;
    }
    // An output of a node that runs, given a name where it ran (inside another branch): its name, never a second call.
    if (const FString* Name = Named.Find(Key))
    {
        return *Name;
    }
    const FNode& Node = Graph.Nodes[NodeIndex];
    if (Path.Contains(Key))
    {
        Warnings.AddUnique(FString::Printf(TEXT("%s feeds its own input; printed as _."), *Node.Id));
        return TEXT("_");
    }
    Placed.Add(NodeIndex);
    if (Node.Kind == EKind::Self)
    {
        return TEXT("self");
    }
    const TArray<const FPin*> Ins = DataIns(Node);
    if (IsMakeLiteral(Node.TypeId) && Ins.Num() > 0 && Ins[0]->Links.Num() == 0)
    {
        return FormatDefault(*Ins[0]);
    }
    Path.Add(Key);
    const auto Arg = [this, &Ins, &Bindings](int32 At) { return Ins.IsValidIndex(At) ? Expr(*Ins[At], Bindings) : FString(TEXT("_")); };
    FString Result;
    if (const TCHAR* Op = OperatorFor(Node.TypeId))
    {
        const FString Left = Arg(0);
        const FString Right = Arg(1);
        Result = FCString::Strcmp(Op, TEXT("-")) == 0 && Left == TEXT("0") ? TEXT("(- ") + Right + TEXT(")")
                                                                           : FString::Printf(TEXT("(%s %s %s)"), Op, *Left, *Right);
    }
    else if (SameText(Node.TypeId, TEXT("Math|Boolean|NOTBoolean")))
    {
        Result = TEXT("(not ") + Arg(0) + TEXT(")");
    }
    else if (Node.Kind == EKind::Select)
    {
        // Option 1 is the value when the condition holds, Option 0 the other.
        const FPin* Condition = nullptr;
        TArray<const FPin*> Values;
        for (const FPin* In : Ins)
        {
            const FString Lower = In->Name.ToLower();
            if (!Condition && (Lower == TEXT("condition") || Lower == TEXT("bpicka") || Lower == TEXT("option") || Lower == TEXT("index")))
            {
                Condition = In;
            }
            else
            {
                Values.Add(In);
            }
        }
        const FString Picked = Condition ? Expr(*Condition, Bindings) : FString(TEXT("true"));
        const FString A = Values.Num() > 1 ? Expr(*Values[1], Bindings) : FString(TEXT("_"));
        const FString B = Values.Num() > 0 ? Expr(*Values[0], Bindings) : FString(TEXT("_"));
        Result = FString::Printf(TEXT("(select %s %s %s)"), *Picked, *A, *B);
    }
    else if (const TCHAR* Attr = ComponentFor(Node.TypeId, Node.Outs[Out].Name))
    {
        Result = FString::Printf(TEXT("(.%s %s)"), Attr, *Arg(0));
    }
    else
    {
        Result = TEXT("(") + CallInner(NodeIndex, Bindings) + TEXT(")");
    }
    Path.Pop();
    return Result;
}

FString FDecompiler::CallInner(int32 NodeIndex, const FBindings& Bindings)
{
    Placed.Add(NodeIndex);
    const FNode& Node = Graph.Nodes[NodeIndex];
    TArray<const FPin*> Ins = DataIns(Node);
    // The implicit target of a call on the Blueprint itself: unwired and empty, it is not an argument.
    if (Ins.Num() > 0 && Ins[0]->Name.Equals(TEXT("self"), ESearchCase::IgnoreCase) && Ins[0]->Links.Num() == 0 && Ins[0]->Value.IsEmpty())
    {
        Ins.RemoveAt(0);
    }
    TArray<FString> Args;
    for (const FPin* In : Ins)
    {
        Args.Add(Expr(*In, Bindings));
    }
    // Trailing arguments still at the value the node was made with are left out; one always stays.
    while (Args.Num() > 1 && Ins[Args.Num() - 1]->Links.Num() == 0 && Ins[Args.Num() - 1]->bAtDefault)
    {
        Args.Pop();
    }
    return Args.Num() > 0 ? Node.TypeId + TEXT(" ") + FString::Join(Args, TEXT(" ")) : Node.TypeId;
}

// A range stop wired as (A - 1) reads back as A: the writer turns (range A) into LastIndex A - 1.
FString FDecompiler::UnwrapDecrement(const FPin& Last, const FBindings& Bindings)
{
    if (Last.Links.Num() == 0)
    {
        return FString();
    }
    const FNode& Minus = Graph.Nodes[Last.Links[0].Node];
    const TCHAR* Op = OperatorFor(Minus.TypeId);
    const TArray<const FPin*> Ins = DataIns(Minus);
    if (!Op || FCString::Strcmp(Op, TEXT("-")) != 0 || Ins.Num() < 2)
    {
        return FString();
    }
    const FPin& One = *Ins[1];
    bool bOne = One.Links.Num() == 0 && One.Value == TEXT("1");
    if (One.Links.Num() > 0 && IsMakeLiteral(Graph.Nodes[One.Links[0].Node].TypeId))
    {
        const TArray<const FPin*> LiteralIns = DataIns(Graph.Nodes[One.Links[0].Node]);
        bOne = LiteralIns.Num() > 0 && LiteralIns[0]->Links.Num() == 0 && LiteralIns[0]->Value == TEXT("1");
    }
    return bOne ? Expr(*Ins[0], Bindings) : FString();
}

FPinRef FDecompiler::FollowDataKnots(FPinRef From, bool& bHalted) const
{
    bHalted = false;
    TSet<int32> Seen;
    while (Graph.Nodes[From.Node].Kind == EKind::Knot)
    {
        const TArray<const FPin*> Ins = DataIns(Graph.Nodes[From.Node]);
        if (Seen.Contains(From.Node) || Ins.Num() == 0 || Ins[0]->Links.Num() == 0)
        {
            bHalted = true;
            return From;
        }
        Seen.Add(From.Node);
        From = Ins[0]->Links[0];
    }
    return From;
}

FString FDecompiler::AllocName(const FString& PinName, const FBindings& Bindings) const
{
    TSet<FString> Used;
    for (const TPair<FKey, FString>& Pair : Bindings)
    {
        Used.Add(Pair.Value);
    }
    const FString Base = TEXT("_") + SanitizePinName(PinName);
    FString Name = Base;
    for (int32 Suffix = 1; Used.Contains(Name); ++Suffix)
    {
        Name = FString::Printf(TEXT("%s_%d"), *Base, Suffix);
    }
    return Name;
}

// Names for the outputs read elsewhere, by position among the node's data outputs (as the writer binds them); an
// output not read before a read one holds its place as _.
TArray<FString> FDecompiler::Targets(int32 NodeIndex, FState& State)
{
    const FNode& Node = Graph.Nodes[NodeIndex];
    TArray<int32> Outs;
    int32 Last = INDEX_NONE;
    for (int32 At = 0; At < Node.Outs.Num(); ++At)
    {
        if (IsDataOut(Node.Outs[At]))
        {
            Last = Needed.Contains(FKey(NodeIndex, At)) ? Outs.Num() : Last;
            Outs.Add(At);
        }
    }
    TArray<FString> Names;
    for (int32 At = 0; At <= Last; ++At)
    {
        const FKey Key(NodeIndex, Outs[At]);
        if (!Needed.Contains(Key))
        {
            Names.Add(TEXT("_"));
            continue;
        }
        const FString Name = AllocName(Node.Outs[Outs[At]].Name, State.Bindings);
        State.Bindings.Add(Key, Name);
        Named.Add(Key, Name);
        Names.Add(Name);
    }
    return Names;
}

} // namespace Emit
} // namespace McpGraphCode
