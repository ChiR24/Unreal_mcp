#include "Domains/BlueprintGraph/Code/McpGraphCodeEmit.h"

namespace McpGraphCode
{
namespace Emit
{
void FDecompiler::WalkPure(const FPin& Pin, TSet<FKey>& Seen) const
{
    bool bHalted = false;
    const FPinRef From = Pin.Links.Num() > 0 ? FollowDataKnots(Pin.Links[0], bHalted) : FPinRef();
    if (From.Node == INDEX_NONE || bHalted)
    {
        return;
    }
    const FNode& Source = Graph.Nodes[From.Node];
    const auto IsExec = [](const FPin& Each) { return Each.bExec; };
    if (Source.Ins.ContainsByPredicate(IsExec) || Source.Outs.ContainsByPredicate(IsExec) || Seen.Contains(FKey(From.Node, From.Pin)))
    {
        return;
    }
    Seen.Add(FKey(From.Node, From.Pin));
    if (!IsMakeLiteral(Source.TypeId))
    {
        for (const FPin* In : DataIns(Source))
        {
            WalkPure(*In, Seen);
        }
    }
}

// Pure outputs that two or more nodes of this event read: bound once at the top, as Epic's reader does.
TSet<FKey> FDecompiler::SharedPure(int32 Entry) const
{
    TArray<int32> Queue = {Entry};
    TSet<int32> Seen;
    TMap<FKey, int32> Readers;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 At = Queue[Head];
        if (Seen.Contains(At))
        {
            continue;
        }
        Seen.Add(At);
        TSet<FKey> Keys;
        for (const FPin* In : DataIns(Graph.Nodes[At]))
        {
            WalkPure(*In, Keys);
        }
        for (const FKey& Key : Keys)
        {
            ++Readers.FindOrAdd(Key);
        }
        for (const FPin& Pin : Graph.Nodes[At].Outs)
        {
            if (Pin.bExec && Pin.Links.Num() > 0)
            {
                Queue.Add(Pin.Links[0].Node);
            }
        }
    }
    TSet<FKey> Shared;
    for (const TPair<FKey, int32>& Pair : Readers)
    {
        if (Pair.Value >= 2)
        {
            Shared.Add(Pair.Key);
        }
    }
    return Shared;
}

// Dependencies before dependents, starting in GUID order so two reads of one graph print the same text.
TArray<FKey> FDecompiler::SortHoisted(const TSet<FKey>& Keys) const
{
    TArray<FKey> Starts = Keys.Array();
    Starts.Sort([this](const FKey& A, const FKey& B)
    {
        const int32 Order = Graph.Nodes[A.Key].Guid.Compare(Graph.Nodes[B.Key].Guid, ESearchCase::CaseSensitive);
        return Order != 0 ? Order < 0 : A.Value < B.Value;
    });
    TArray<FKey> Sorted;
    TSet<FKey> Entered;
    TSet<FKey> Done;
    for (const FKey& Start : Starts)
    {
        TArray<TPair<FKey, bool>> Stack = {TPair<FKey, bool>(Start, false)};
        while (Stack.Num() > 0)
        {
            const TPair<FKey, bool> Top = Stack.Pop();
            if (Top.Value && !Done.Contains(Top.Key))
            {
                Done.Add(Top.Key);
                Sorted.Add(Top.Key);
            }
            if (Top.Value || Entered.Contains(Top.Key))
            {
                continue;
            }
            Entered.Add(Top.Key);
            Stack.Add(TPair<FKey, bool>(Top.Key, true));
            for (const FPin* In : DataIns(Graph.Nodes[Top.Key.Key]))
            {
                bool bHalted = false;
                const FPinRef Dep = In->Links.Num() > 0 ? FollowDataKnots(In->Links[0], bHalted) : FPinRef();
                const FKey DepKey(Dep.Node, Dep.Pin);
                if (Dep.Node != INDEX_NONE && !bHalted && Keys.Contains(DepKey) && !Entered.Contains(DepKey))
                {
                    Stack.Add(TPair<FKey, bool>(DepKey, false));
                }
            }
        }
    }
    return Sorted;
}
FString FDecompiler::Run(TSet<int32>& OutPlaced, TArray<FString>& OutWarnings)
{
    // Entries: nodes the flow starts from (an exec output, no exec input), top to bottom, then by GUID.
    TArray<int32> Entries;
    const auto HasExec = [](const TArray<FPin>& Pins) { return Pins.ContainsByPredicate([](const FPin& Pin) { return Pin.bExec; }); };
    for (int32 At = 0; At < Graph.Nodes.Num(); ++At)
    {
        if (IsEntry(Graph.Nodes[At]))
        {
            Entries.Add(At);
        }
    }
    Entries.StableSort([this](int32 A, int32 B)
    {
        const FNode& Left = Graph.Nodes[A];
        const FNode& Right = Graph.Nodes[B];
        return Left.Y != Right.Y ? Left.Y < Right.Y : Left.Guid.Compare(Right.Guid, ESearchCase::CaseSensitive) < 0;
    });

    // An output of a node that runs (exec in and out) and is read elsewhere gets a name where the node runs.
    TArray<int32> Queue = Entries;
    TSet<int32> Seen;
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 At = Queue[Head];
        if (Seen.Contains(At))
        {
            continue;
        }
        Seen.Add(At);
        const FNode& Node = Graph.Nodes[At];
        const bool bRuns = HasExec(Node.Ins) && HasExec(Node.Outs);
        for (int32 Out = 0; Out < Node.Outs.Num(); ++Out)
        {
            const FPin& Pin = Node.Outs[Out];
            if (bRuns && IsDataOut(Pin) && Pin.Links.Num() > 0)
            {
                Needed.Add(FKey(At, Out));
            }
            if (Pin.bExec && Pin.Links.Num() > 0)
            {
                Queue.Add(Pin.Links[0].Node);
            }
        }
    }

    TArray<FLine> Lines;
    for (const int32 Entry : Entries)
    {
        const FNode& Node = Graph.Nodes[Entry];
        Visited.Add(Entry);
        Placed.Add(Entry);
        const bool bEvent = Node.TypeId.StartsWith(TEXT("AddEvent|"), ESearchCase::CaseSensitive);
        const FString Name = EntryName(Node.TypeId);
        TArray<FString> Params;
        FBindings Bindings;
        for (int32 Out = 0; Out < Node.Outs.Num(); ++Out)
        {
            if (IsDataOut(Node.Outs[Out]))
            {
                Params.Add(Node.Outs[Out].Name);
                Bindings.Add(FKey(Entry, Out), Node.Outs[Out].Name);
            }
        }
        const FString ParamList = TEXT("(") + FString::Join(Params, TEXT(" ")) + TEXT(")");
        const bool bFunction = Node.TypeId.StartsWith(TEXT("|"), ESearchCase::CaseSensitive) && !bEvent;
        Lines.Add({bFunction ? FString::Printf(TEXT("(fn %s %s"), *Name, *ParamList)
                             : Params.Num() > 0 ? FString::Printf(TEXT("(event %s %s"), *Name, *ParamList) : TEXT("(event ") + Name,
                   Node.Id});
        // A pure value two nodes read is bound once at the top; Blueprint still works it out again at each read.
        for (const FKey& Key : SortHoisted(SharedPure(Entry)))
        {
            const FString Var = AllocName(Graph.Nodes[Key.Key].Outs[Key.Value].Name, Bindings);
            const FString Value = Source(Key.Key, Key.Value, Bindings);
            Bindings.Add(Key, Var);
            Lines.Add({FString::Printf(TEXT("  (bind %s %s)"), *Var, *Value), Graph.Nodes[Key.Key].Id + TEXT(" pure, runs at each read")});
        }
        FState State{Bindings, 1, {}};
        const FPinRef Then = FollowExec(Node, TEXT("then"));
        if (Then.Node != INDEX_NONE)
        {
            Stmts(Then, State);
        }
        for (const FForm& Form : State.Lines)
        {
            Form.Render(Lines);
        }
        Lines.Last().Text += TEXT(")");
        Lines.Add({});
    }
    OutPlaced.Append(Placed);
    OutWarnings.Append(Warnings);
    TArray<FString> Text;
    for (const FLine& Line : Lines)
    {
        const bool bCommentOnly = Line.Text.TrimEnd().IsEmpty();
        Text.Add(Line.Tag.IsEmpty() ? Line.Text : Line.Text + (bCommentOnly ? TEXT("; ") : TEXT("  ; ")) + Line.Tag);
    }
    return FString::Join(Text, TEXT("\n"));
}
} // namespace Emit

FString Decompile(const FGraph& Graph, TSet<int32>& OutPlaced, TArray<FString>& OutWarnings)
{
    Emit::FDecompiler Decompiler(Graph);
    return Decompiler.Run(OutPlaced, OutWarnings);
}
} // namespace McpGraphCode
