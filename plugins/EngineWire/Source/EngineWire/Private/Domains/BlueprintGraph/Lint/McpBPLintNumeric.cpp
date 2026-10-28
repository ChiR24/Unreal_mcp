// divide_by_zero, index_out_of_range, out_of_range, constant_condition and endless_loop, judged on the states the
// abstract interpreter finds (Interp/McpBPLintInterp.cpp); float_equality read from the graph alone.

#include "Domains/BlueprintGraph/Lint/McpBPLintNumeric.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "Domains/BlueprintGraph/Lint/McpBPLintRules.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"

namespace McpBPLint
{
namespace
{
// A value a function works out (arithmetic, a read from the world): not a literal, a variable, or an exact conversion.
bool IsWorkedOut(const UEdGraphPin* Input)
{
    const UEdGraphPin* Source = SourceOf(Input);
    const UK2Node_CallFunction* Call = Source ? Cast<UK2Node_CallFunction>(Source->GetOwningNode()) : nullptr;
    const FString Name = FunctionName(Call).ToString();
    return Call && !Name.StartsWith(TEXT("MakeLiteral")) && !(Name.StartsWith(TEXT("Conv_")) && (Name.Contains(TEXT("Int")) || Name.Contains(TEXT("Byte"))));
}

void FloatEquality(const TArray<UEdGraph*>& Graphs, TArray<FFinding>& Out)
{
    for (const UEdGraph* Graph : Graphs)
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
            const TArray<const UEdGraphPin*> In = Call ? DataInputs(Call) : TArray<const UEdGraphPin*>();
            FString Op;
            bool bInteger = true;
            if (In.Num() == 2 && ScalarOp(Call, Op, bInteger) && !bInteger && (Op == TEXT("EqualEqual") || Op == TEXT("NotEqual")) &&
                (IsWorkedOut(In[0]) || IsWorkedOut(In[1])))
            {
                Out.Add({TEXT("float_equality"), TEXT("warning"), Call, TEXT("== on a float worked out by arithmetic or read from the world is rarely exactly true (0.1 + 0.2 is not 0.3): compare with Nearly Equal and a tolerance.")});
            }
        }
    }
}

void Report(const FVerdicts& Verdicts, TFunctionRef<bool(const TCHAR*)> On, TArray<FFinding>& Out)
{
    static const TCHAR* const Valued[] = {TEXT("divide_by_zero"), TEXT("index_out_of_range"), TEXT("out_of_range")};
    for (const TPair<const UEdGraphNode*, TMap<FString, FVerdict>>& Node : Verdicts)
    {
        for (const TCHAR* Rule : Valued)
        {
            const FVerdict* Verdict = Node.Value.Find(Rule);
            if (Verdict && Verdict->bMay && On(Rule))
            {
                const bool bError = Verdict->bMust && FCString::Strcmp(Rule, TEXT("out_of_range")) != 0;
                Out.Add({Rule, bError ? TEXT("error") : TEXT("warning"), Node.Key, Verdict->bMust ? Verdict->MustDetail : Verdict->Detail});
            }
        }
        for (const bool bTrue : {true, false})
        {
            const FVerdict* Verdict = Node.Value.Find(bTrue ? TEXT("always_true") : TEXT("always_false"));
            const UEdGraphPin* Dead = Verdict && Verdict->bMust ? Node.Key->FindPin(bTrue ? UEdGraphSchema_K2::PN_Else : UEdGraphSchema_K2::PN_Then) : nullptr;
            if (Dead && Dead->LinkedTo.Num() > 0 && On(TEXT("constant_condition")))
            {
                Out.Add({TEXT("constant_condition"), TEXT("warning"), Node.Key, bTrue
                    ? TEXT("This condition is always true where it runs, so its False exit never runs.")
                    : TEXT("This condition is always false where it runs, so its True exit never runs.")});
            }
        }
        const FVerdict* Endless = Node.Value.Find(TEXT("endless_loop"));
        if (Endless && Endless->bMust && On(TEXT("endless_loop")))
        {
            Out.Add({TEXT("endless_loop"), TEXT("warning"), Node.Key, TEXT("This While Loop's condition is still true after every pass through its body: nothing in the body can make it false, so once it starts it never ends and Play stops it as an infinite loop.")});
        }
    }
}
} // namespace

int32 NumericRules(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TFunctionRef<bool(const TCHAR*)> On, TArray<FFinding>& Out)
{
    if (On(TEXT("float_equality")))
    {
        FloatEquality(Graphs, Out);
    }
    const bool bValues = On(TEXT("divide_by_zero")) || On(TEXT("index_out_of_range")) || On(TEXT("out_of_range"));
    const bool bDecisions = On(TEXT("constant_condition")) || On(TEXT("endless_loop"));
    if (!bValues && !bDecisions)
    {
        return 0;
    }
    const FMembers Members = ReadMembers(Blueprint, Graphs);
    // A Value Range says what other Blueprints should store, not what they do: no condition is decided by one.
    FMembers Unranged = Members;
    for (const TPair<FName, FRange>& Range : Members.Ranges)
    {
        Unranged.Entry.Numbers.Remove(Range.Key);
    }
    FVerdicts Verdicts;
    int32 Skipped = 0;
    for (const UEdGraph* Graph : Graphs)
    {
        int32 GraphSkipped = 0;
        if (bValues)
        {
            const FAnalysis Analysis = Interpret(Graph, Members);
            for (const FRun& Run : Analysis.Runs)
            {
                CheckValues(Run, Members, Verdicts);
            }
            GraphSkipped = Analysis.Skipped;
        }
        if (bDecisions)
        {
            const FAnalysis Analysis = Interpret(Graph, Unranged);
            CheckDecisions(Analysis, Verdicts);
            GraphSkipped = FMath::Max(GraphSkipped, Analysis.Skipped);
        }
        Skipped += GraphSkipped;
    }
    Report(Verdicts, On, Out);
    return Skipped;
}
} // namespace McpBPLint
