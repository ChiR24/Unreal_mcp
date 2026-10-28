// How the lint's interpreter reads a pure input where an exec node runs: literals, variables in the state, loop indexes,
// and the math, comparison and array functions of the engine's libraries by name (the 5.0 FloatFloat names and the later
// DoubleDouble ones alike). Anything else is unknown.

#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintEval.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MakeArray.h"

#include <cmath>

namespace McpBPLint
{
namespace
{
// A pin's own value when nothing is wired into it.
FRange LiteralValue(const UEdGraphPin* Pin)
{
    if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        return FRange::Exactly(Pin->DefaultValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? 1.0 : 0.0);
    }
    return IsNumberPin(Pin) ? FRange::Exactly(FCString::Atod(*Pin->DefaultValue)) : FRange::Unknown(true);
}

FRange FoldInputs(const FEval& Eval, const TArray<const UEdGraphPin*>& In, int32 Depth, FRange (*Combine)(const FRange&, const FRange&))
{
    FRange Value = In.Num() > 0 ? Eval.Number(In[0], Depth) : FRange::Unknown(true);
    for (int32 Index = 1; Index < In.Num(); ++Index)
    {
        Value = Combine(Value, Eval.Number(In[Index], Depth));
    }
    return Value;
}

FRange AndOf(const FRange& A, const FRange& B)
{
    return NumbersOnly(MinOf(A, B));
}

FRange OrOf(const FRange& A, const FRange& B)
{
    return NumbersOnly(MaxOf(A, B));
}

FRange RoundedBy(const FString& Name, const FRange& Value)
{
    double (*Round)(double) = [](double V) { return std::trunc(V); };
    if (Name.Contains(TEXT("Floor")))
    {
        Round = [](double V) { return std::floor(V); };
    }
    else if (Name.Contains(TEXT("Ceil")))
    {
        Round = [](double V) { return std::ceil(V); };
    }
    else if (Name.Contains(TEXT("Round")))
    {
        Round = [](double V) { return std::round(V); };
    }
    return RoundOf(Value, Round);
}

FRange ScalarValue(const FEval& Eval, const TArray<const UEdGraphPin*>& In, int32 Depth, const FString& Op, bool bInteger)
{
    const auto Arg = [&Eval, &In, Depth](int32 Index) { return In.IsValidIndex(Index) ? Eval.Number(In[Index], Depth) : FRange::Unknown(true); };
    ECompare Test = ECompare::Equal;
    if (Op == TEXT("Add") || Op == TEXT("Multiply"))
    {
        return FoldInputs(Eval, In, Depth, Op == TEXT("Add") ? &Add : &Mul);
    }
    if (Op == TEXT("Subtract") && In.Num() == 2)
    {
        // A check that compared the two (Health >= Damage, A > B) bounds their difference where intervals cannot.
        const FRange Difference = Sub(Arg(0), Arg(1));
        const FRange* Fact = Eval.State.Differences.Find(TPair<FName, FName>(ValueKey(In[0]), ValueKey(In[1])));
        return Fact ? Meet(Difference, *Fact) : Difference;
    }
    if (Op == TEXT("Divide") || Op == TEXT("Percent"))
    {
        return Op == TEXT("Divide") ? Div(Arg(0), Arg(1), bInteger) : Rem(Arg(0), Arg(1));
    }
    return CompareOp(Op, Test) && In.Num() == 2 ? Compare(Test, Arg(0), Arg(1)) : FRange::Unknown(true);
}

// What a pure library call gives, from its inputs' values.
FRange LibraryValue(const FEval& Eval, const UK2Node_CallFunction* Call, int32 Depth)
{
    const TArray<const UEdGraphPin*> In = DataInputs(Call);
    const auto Arg = [&Eval, &In, Depth](int32 Index) { return In.IsValidIndex(Index) ? Eval.Number(In[Index], Depth) : FRange::Unknown(true); };
    FString Op;
    bool bInteger = false;
    if (ScalarOp(Call, Op, bInteger))
    {
        return ScalarValue(Eval, In, Depth, Op, bInteger);
    }
    const FString Name = FunctionName(Call).ToString();
    if (Name == TEXT("Not_PreBool"))
    {
        const FRange Value = Arg(0);
        FRange Not = Value.bNone ? Value : FRange::Between(1.0 - Value.Hi, 1.0 - Value.Lo);
        Not.bOpaque = Value.bOpaque;
        return Not;
    }
    if (Name == TEXT("BooleanAND") || Name == TEXT("BooleanOR"))
    {
        return FoldInputs(Eval, In, Depth, Name == TEXT("BooleanAND") ? &AndOf : &OrOf);
    }
    if (Name == TEXT("Max") || Name == TEXT("FMax") || Name == TEXT("MaxInt64") || Name == TEXT("Min") || Name == TEXT("FMin") || Name == TEXT("MinInt64"))
    {
        return FoldInputs(Eval, In, Depth, Name.Contains(TEXT("Max")) ? &MaxOf : &MinOf);
    }
    if (Name == TEXT("Clamp") || Name == TEXT("FClamp") || Name == TEXT("ClampInt64"))
    {
        return MinOf(MaxOf(Arg(0), Arg(1)), Arg(2));
    }
    if (Name.StartsWith(TEXT("Abs")))
    {
        return AbsOf(Arg(0));
    }
    // Kept against array lengths as a loop's index is: Random Integer In Range(0, Length) reaches one past the end.
    if (Name == TEXT("RandomIntegerInRange") || Name == TEXT("RandomFloatInRange") || Name == TEXT("RandomInteger64InRange"))
    {
        const bool bWhole = Name != TEXT("RandomFloatInRange");
        return Refine(Refine(FRange::Unknown(Arg(0).bOpaque || Arg(1).bOpaque), ECompare::GreaterEqual, Arg(0), bWhole), ECompare::LessEqual, Arg(1), bWhole);
    }
    if (Name == TEXT("RandomInteger"))
    {
        const FRange Max = Arg(0);
        return MaxOf(Refine(FRange::Unknown(Max.bOpaque), ECompare::Less, Max, true), FRange::Exactly(0.0)); // 0 when Max <= 0
    }
    if (Name == TEXT("RandomFloat"))
    {
        return FRange::Between(0.0, 1.0);
    }
    if (Name.StartsWith(TEXT("MakeLiteral")) || (Name.StartsWith(TEXT("Conv_")) && In.Num() == 1 && IsNumberPin(In[0])))
    {
        return Arg(0);
    }
    if (Name.StartsWith(TEXT("FFloor")) || Name.StartsWith(TEXT("FCeil")) || Name.StartsWith(TEXT("FTrunc")) || Name.StartsWith(TEXT("Round")))
    {
        return RoundedBy(Name, Arg(0));
    }
    if ((Name == TEXT("Array_Length") || Name == TEXT("Array_LastIndex")) && In.Num() == 1)
    {
        const FRange Length = Eval.Array(In[0]).Length;
        return Name == TEXT("Array_Length") ? Length : Sub(Length, FRange::Exactly(1.0));
    }
    return FRange::Unknown(true);
}
} // namespace

bool CompareOp(const FString& Op, ECompare& OutTest)
{
    static const TPair<const TCHAR*, ECompare> Tests[] = {{TEXT("Less"), ECompare::Less}, {TEXT("LessEqual"), ECompare::LessEqual},
        {TEXT("Greater"), ECompare::Greater}, {TEXT("GreaterEqual"), ECompare::GreaterEqual},
        {TEXT("EqualEqual"), ECompare::Equal}, {TEXT("NotEqual"), ECompare::NotEqual}};
    for (const TPair<const TCHAR*, ECompare>& Test : Tests)
    {
        if (Op == Test.Key)
        {
            OutTest = Test.Value;
            return true;
        }
    }
    return false;
}

FRange FEval::Number(const UEdGraphPin* Input, int32 Depth) const
{
    if (!Input || Depth > MaxEvalDepth)
    {
        return FRange::Unknown(true);
    }
    if (Input->LinkedTo.Num() == 0)
    {
        return LiteralValue(Input);
    }
    const FName Key = ValueKey(Input);
    if (!Key.IsNone())
    {
        const FRange* Known = State.Numbers.Find(Key);
        return Known ? *Known : FRange::Unknown(true);
    }
    const UEdGraphPin* Source = SourceOf(Input);
    const UEdGraphNode* Node = Source ? Source->GetOwningNode() : nullptr;
    if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
    {
        return Call->IsNodePure() ? LibraryValue(*this, Call, Depth + 1) : FRange::Unknown(true);
    }
    // A function's or custom event's inputs are whatever its callers pass: unknown, but on this graph's own account.
    return FRange::Unknown(!(Cast<UK2Node_FunctionEntry>(Node) || Cast<UK2Node_CustomEvent>(Node)));
}

FArrayRead FEval::Array(const UEdGraphPin* Input) const
{
    FArrayRead Read;
    Read.Length = FRange::Between(0.0, Unbounded);
    Read.Length.bOpaque = true;
    const UEdGraphPin* Source = SourceOf(Input);
    Read.Variable = ReadVariable(Input);
    if (!Read.Variable.IsNone())
    {
        const FRange* Known = State.Lengths.Find(Read.Variable);
        Read.Length = FRange::LengthOf(Read.Variable, Known ? *Known : Read.Length);
    }
    else if (const UK2Node_MakeArray* Make = Source ? Cast<UK2Node_MakeArray>(Source->GetOwningNode()) : nullptr)
    {
        Read.Length = FRange::Exactly(DataInputs(Make).Num());
    }
    return Read;
}
} // namespace McpBPLint
