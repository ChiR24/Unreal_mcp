// The interpreter's states (join, widening), and what a condition tells of them on each of its exits.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintEval.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"

namespace McpBPLint
{
namespace
{
template <typename KeyType>
bool SameRanges(const TMap<KeyType, FRange>& A, const TMap<KeyType, FRange>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const TPair<KeyType, FRange>& Entry : A)
    {
        const FRange* Other = B.Find(Entry.Key);
        if (!Other || !SameRange(Entry.Value, *Other))
        {
            return false;
        }
    }
    return true;
}

// The keys both know, each combined; one only one side knows is unknown on the other, so it drops out.
template <typename KeyType>
TMap<KeyType, FRange> MergeRanges(const TMap<KeyType, FRange>& Old, const TMap<KeyType, FRange>& New, bool bWiden)
{
    TMap<KeyType, FRange> Out;
    for (const TPair<KeyType, FRange>& Entry : Old)
    {
        if (const FRange* Other = New.Find(Entry.Key))
        {
            Out.Add(Entry.Key, bWiden ? Widen(Entry.Value, *Other) : Join(Entry.Value, *Other));
        }
    }
    return Out;
}

FState MergeStates(const FState& Old, const FState& New, bool bWiden)
{
    if (!Old.bLive || !New.bLive)
    {
        return Old.bLive ? Old : New;
    }
    FState Out;
    Out.bLive = true;
    Out.Numbers = MergeRanges(Old.Numbers, New.Numbers, bWiden);
    Out.Lengths = MergeRanges(Old.Lengths, New.Lengths, bWiden);
    Out.Differences = MergeRanges(Old.Differences, New.Differences, bWiden);
    return Out;
}

// Key narrowed to the values where (Key Op Other) holds; false when none is left.
bool Narrow(FState& Out, FName Key, ECompare Op, const FRange& Other, bool bInteger)
{
    if (Key.IsNone())
    {
        return true;
    }
    const FRange* Known = Out.Numbers.Find(Key);
    const FRange Narrowed = Refine(Known ? *Known : FRange::Unknown(true), Op, Other, bInteger);
    if (Narrowed.bNone)
    {
        return false;
    }
    Out.Numbers.Add(Key, Narrowed);
    return true;
}

// Left Op Right holds, both variables the state keys: Left - Right Op 0, kept both ways round. Floats keep strictness
// through bNonZero: with gradual underflow, A > B means A - B is never 0.
void RecordDifference(FState& Out, FName Left, ECompare Op, FName Right, bool bInteger)
{
    if (Left.IsNone() || Right.IsNone() || Left == Right)
    {
        return;
    }
    const TPair<FName, FName> Pair(Left, Right);
    const FRange* Known = Out.Differences.Find(Pair);
    const FRange Holds = Refine(FRange::Unknown(false), Op, FRange::Exactly(0.0), bInteger);
    const FRange Fact = Known ? Meet(*Known, Holds) : Holds;
    Out.Differences.Add(Pair, Fact);
    Out.Differences.Add(TPair<FName, FName>(Right, Left), Negate(Fact));
}
} // namespace

bool SameState(const FState& A, const FState& B)
{
    return A.bLive == B.bLive && SameRanges(A.Numbers, B.Numbers) && SameRanges(A.Lengths, B.Lengths) &&
        SameRanges(A.Differences, B.Differences);
}

FState JoinStates(const FState& A, const FState& B)
{
    return MergeStates(A, B, false);
}

FState WidenStates(const FState& Old, const FState& New)
{
    return MergeStates(Old, New, true);
}

void ForgetLength(FState& State, FName Array)
{
    for (TPair<FName, FRange>& Entry : State.Numbers)
    {
        if (Entry.Value.Array == Array)
        {
            Entry.Value = NumbersOnly(Entry.Value);
        }
    }
}

void ForgetDifferences(FState& State, FName Variable)
{
    for (auto It = State.Differences.CreateIterator(); It; ++It)
    {
        if (It->Key.Key == Variable || It->Key.Value == Variable)
        {
            It.RemoveCurrent();
        }
    }
}

bool FEval::Assume(FState& Out, const UEdGraphPin* Condition, bool bTrue, int32 Depth) const
{
    const FRange Value = Number(Condition, Depth);
    if (Value.bNone || Value.IsExactly(bTrue ? 0.0 : 1.0))
    {
        return false; // this exit never runs
    }
    if (!Condition || Condition->LinkedTo.Num() == 0 || Depth > MaxEvalDepth)
    {
        return true;
    }
    const FName Key = ValueKey(Condition);
    if (!Key.IsNone())
    {
        return Narrow(Out, Key, ECompare::Equal, FRange::Exactly(bTrue ? 1.0 : 0.0), true);
    }
    const UEdGraphPin* Source = SourceOf(Condition);
    const UK2Node_CallFunction* Call = Source ? Cast<UK2Node_CallFunction>(Source->GetOwningNode()) : nullptr;
    if (!Call || !Call->IsNodePure())
    {
        return true;
    }
    const TArray<const UEdGraphPin*> In = DataInputs(Call);
    const FString Name = FunctionName(Call).ToString();
    if (Name == TEXT("Not_PreBool") && In.Num() == 1)
    {
        return Assume(Out, In[0], !bTrue, Depth + 1);
    }
    if ((Name == TEXT("BooleanAND") && bTrue) || (Name == TEXT("BooleanOR") && !bTrue))
    {
        for (const UEdGraphPin* Part : In)
        {
            if (!Assume(Out, Part, bTrue, Depth + 1))
            {
                return false;
            }
        }
        return true;
    }
    if (Name == TEXT("Array_IsValidIndex") && bTrue && In.Num() == 2)
    {
        const FRange Length = Array(In[0]).Length;
        return Narrow(Out, ValueKey(In[1]), ECompare::GreaterEqual, FRange::Exactly(0.0), true) &&
            Narrow(Out, ValueKey(In[1]), ECompare::Less, Length, true);
    }
    FString Op;
    bool bInteger = false;
    ECompare Test = ECompare::Equal;
    if (In.Num() == 2 && ScalarOp(Call, Op, bInteger) && CompareOp(Op, Test))
    {
        const ECompare Holds = bTrue ? Test : Inverse(Test);
        const FRange Left = Number(In[0], Depth + 1);
        const FRange Right = Number(In[1], Depth + 1);
        RecordDifference(Out, ValueKey(In[0]), Holds, ValueKey(In[1]), bInteger);
        return Narrow(Out, ValueKey(In[0]), Holds, Right, bInteger) && Narrow(Out, ValueKey(In[1]), Mirror(Holds), Left, bInteger);
    }
    return true;
}
} // namespace McpBPLint
