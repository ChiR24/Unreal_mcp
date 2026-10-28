// The numeric checks of one run of an exec node (McpBPLintNumeric.cpp gathers them): each says whether its problem may
// happen in that run and whether it must, so a node is reported when some run can go wrong, and as an error only when
// every run does.

#include "Domains/BlueprintGraph/Lint/McpBPLintNumeric.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableSet.h"

#include <cmath>

namespace McpBPLint
{
namespace
{
constexpr int32 MaxCheckedPureNodes = 256;

FString AmountText(double Value)
{
    return Value == std::round(Value) && FMath::Abs(Value) < 1.0e15 ? FString::Printf(TEXT("%lld"), static_cast<long long>(Value))
                                                                       : FString::Printf(TEXT("%g"), Value);
}

FString RangeText(const FRange& Range)
{
    if (Range.Lo <= -Unbounded && Range.Hi >= Unbounded)
    {
        return TEXT("any value");
    }
    if (Range.Lo <= -Unbounded || Range.Hi >= Unbounded)
    {
        return Range.Lo <= -Unbounded ? TEXT("up to ") + AmountText(Range.Hi) : AmountText(Range.Lo) + TEXT(" or more");
    }
    return Range.Lo == Range.Hi ? AmountText(Range.Lo) : AmountText(Range.Lo) + TEXT(" to ") + AmountText(Range.Hi);
}

// The pure nodes whose values Node reads, each worked out again where Node runs.
TArray<const UEdGraphNode*> PureInputsOf(const UEdGraphNode* Node)
{
    TArray<const UEdGraphNode*> Found;
    TArray<const UEdGraphNode*> Open = {Node};
    while (Open.Num() > 0 && Found.Num() < MaxCheckedPureNodes)
    {
        for (const UEdGraphPin* Pin : Open.Pop()->Pins)
        {
            const UEdGraphPin* Source = Pin && Pin->Direction == EGPD_Input && !IsExecPin(Pin) ? SourceOf(Pin) : nullptr;
            const UEdGraphNode* From = Source ? Source->GetOwningNode() : nullptr;
            if (From && !HasExecPin(From) && !Found.Contains(From))
            {
                Found.Add(From);
                Open.Add(From);
            }
        }
    }
    return Found;
}

void CheckDivisor(const UK2Node_CallFunction* Call, const FEval& Eval, FVerdicts& Verdicts)
{
    FString Op;
    bool bInteger = false;
    const TArray<const UEdGraphPin*> In = DataInputs(Call);
    if (!ScalarOp(Call, Op, bInteger) || (Op != TEXT("Divide") && Op != TEXT("Percent")) || In.Num() != 2)
    {
        return;
    }
    const FRange Divisor = Eval.Number(In[1]);
    const bool bMust = Divisor.IsExactly(0.0);
    Note(Verdicts, Call, TEXT("divide_by_zero"), bMust || (Divisor.CanBeZero() && !Divisor.bOpaque), bMust, bMust
        ? FString(TEXT("The divisor is always 0 here: the log reports Divide by zero and the result is 0."))
        : FString::Printf(TEXT("The divisor can be 0 here (it can be %s): nothing on the way rules 0 out. Check it first, with a Branch on it > 0 or != 0."), *RangeText(Divisor)));
}

void CheckIndex(const UEdGraphNode* Node, const UEdGraphPin* ArrayPin, const UEdGraphPin* IndexPin, const FEval& Eval, FVerdicts& Verdicts)
{
    if (!ArrayPin || !IndexPin)
    {
        return;
    }
    const FRange Index = Eval.Number(IndexPin);
    const FArrayRead Array = Eval.Array(ArrayPin);
    const FString Name = Array.Variable.IsNone() ? FString(TEXT("the array")) : Array.Variable.ToString();
    const bool bSame = !Array.Variable.IsNone() && Index.Array == Array.Variable;
    const bool bKnown = Array.Length.Hi < Unbounded && !Array.Length.bOpaque;
    const bool bPastMay = (bSame && Index.bLenHi && Index.LenHi >= 0.0) || (bKnown && !Index.bOpaque && Index.Hi >= Array.Length.Lo);
    const bool bPastMust = (bSame && Index.bLenLo && Index.LenLo >= 0.0) || (bKnown && Index.Lo >= Array.Length.Hi);
    const bool bNegative = Index.Lo < 0.0 && (!Index.bOpaque || Index.Hi < 0.0);
    FString Detail;
    if (bNegative)
    {
        Detail = FString::Printf(TEXT("The index can be negative here (it can be %s): no item of %s has it."), *RangeText(Index), *Name);
    }
    else if (bSame && Index.bLenHi)
    {
        Detail = FString::Printf(TEXT("The index can reach the length of %s, one past its last item: loop to Last Index rather than Length, or check Is Valid Index first."), *Name);
    }
    else
    {
        Detail = FString::Printf(TEXT("The index can be %s, but %s holds %s item(s)."), *RangeText(Index), *Name, *RangeText(NumbersOnly(Array.Length)));
    }
    Note(Verdicts, Node, TEXT("index_out_of_range"), bNegative || bPastMay || bPastMust, Index.Hi < 0.0 || bPastMust, Detail);
}

void CheckStoredRange(const UK2Node_VariableSet* Set, const FEval& Eval, const FMembers& Members, FVerdicts& Verdicts)
{
    const FRange* Range = Set->VariableReference.IsSelfContext() ? Members.Ranges.Find(Set->GetVarName()) : nullptr;
    const UEdGraphPin* Value = Range ? Set->FindPin(Set->GetVarName(), EGPD_Input) : nullptr;
    if (!Value)
    {
        return;
    }
    const FRange Stored = Eval.Number(Value);
    const bool bMust = !Stored.bNone && (Stored.Hi < Range->Lo || Stored.Lo > Range->Hi);
    Note(Verdicts, Set, TEXT("out_of_range"), bMust || (!Stored.bOpaque && (Stored.Lo < Range->Lo || Stored.Hi > Range->Hi)), bMust,
        FString::Printf(TEXT("%s can be set to %s here, outside its Value Range (%s to %s): the range clamps only the Details panel, never a Set node. Clamp the value before setting it."),
            *Set->GetVarName().ToString(), *RangeText(NumbersOnly(Stored)), *AmountText(Range->Lo), *AmountText(Range->Hi)));
}
} // namespace

void Note(FVerdicts& Verdicts, const UEdGraphNode* Node, const TCHAR* Rule, bool bMay, bool bMust, const FString& Detail)
{
    FVerdict& Verdict = Verdicts.FindOrAdd(Node).FindOrAdd(Rule);
    Verdict.Detail = bMay && !Verdict.bMay ? Detail : Verdict.Detail;
    Verdict.MustDetail = bMust ? Detail : Verdict.MustDetail;
    Verdict.bMay |= bMay;
    Verdict.bMust &= bMust;
}

void CheckValues(const FRun& Run, const FMembers& Members, FVerdicts& Verdicts)
{
    const FEval Eval{Run.State};
    for (const UEdGraphNode* Node : PureInputsOf(Run.Node))
    {
        if (const UK2Node_GetArrayItem* Get = Cast<UK2Node_GetArrayItem>(Node))
        {
            if (Get->Pins.Num() >= 2) // the pin getters index Pins directly; a node that failed to load has none
            {
                CheckIndex(Get, Get->GetTargetArrayPin(), Get->GetIndexPin(), Eval, Verdicts);
            }
        }
        else if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
        {
            CheckDivisor(Call, Eval, Verdicts);
            if (FunctionName(Call) == TEXT("Array_Get"))
            {
                CheckIndex(Call, Call->FindPin(TEXT("TargetArray")), Call->FindPin(TEXT("Index")), Eval, Verdicts);
            }
        }
    }
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Run.Node);
    const FName Function = FunctionName(Call);
    if (Function == TEXT("Array_Set") || Function == TEXT("Array_Remove"))
    {
        CheckIndex(Call, Call->FindPin(TEXT("TargetArray")), Call->FindPin(Function == TEXT("Array_Set") ? TEXT("Index") : TEXT("IndexToRemove")), Eval, Verdicts);
    }
    if (const UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Run.Node))
    {
        CheckStoredRange(Set, Eval, Members, Verdicts);
    }
}

void CheckDecisions(const FAnalysis& Analysis, FVerdicts& Verdicts)
{
    for (const FRun& Run : Analysis.Runs)
    {
        const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Run.Node);
        const UEdGraphPin* Condition = Branch ? Branch->GetConditionPin() : nullptr;
        if (Condition && Condition->LinkedTo.Num() > 0) // a literal condition is the author's own switch
        {
            const FRange Value = FEval{Run.State}.Number(Condition);
            Note(Verdicts, Branch, TEXT("always_true"), false, Value.IsExactly(1.0), FString());
            Note(Verdicts, Branch, TEXT("always_false"), false, Value.IsExactly(0.0), FString());
        }
    }
    for (const TPair<const UEdGraphNode*, TArray<FState>>& Loop : Analysis.LoopReturns)
    {
        const UEdGraphPin* Condition = MacroName(Loop.Key) == TEXT("WhileLoop") ? Loop.Key->FindPin(TEXT("Condition"), EGPD_Input) : nullptr;
        if (!Condition || Condition->LinkedTo.Num() == 0) // a literal true is infinite_while's
        {
            continue;
        }
        for (const FState& Back : Loop.Value)
        {
            Note(Verdicts, Loop.Key, TEXT("endless_loop"), false, FEval{Back}.Number(Condition).IsExactly(1.0), FString());
        }
    }
}
} // namespace McpBPLint
