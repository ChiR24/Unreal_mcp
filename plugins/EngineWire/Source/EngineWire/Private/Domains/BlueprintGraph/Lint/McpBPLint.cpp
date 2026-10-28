#include "Domains/BlueprintGraph/Lint/McpBPLint.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintRules.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "McpBlueprintLint.h"
#include "Templates/Function.h"

namespace McpBPLint
{
namespace
{
const TCHAR* const Rules[] = {TEXT("tick_heavy"), TEXT("unreachable"), TEXT("random_fanout"), TEXT("infinite_while"),
                              TEXT("never_assigned"), TEXT("unguarded_read"), TEXT("divide_by_zero"), TEXT("index_out_of_range"),
                              TEXT("out_of_range"), TEXT("float_equality"), TEXT("constant_condition"), TEXT("endless_loop")};

// The graphs that run: event graphs, functions and what they collapse; macro and delegate signatures are not code of
// this Blueprint. Each rule On allows adds what it finds; the graphs checked come back, and OutSkipped counts the events
// and functions the value rules gave up on.
TArray<UEdGraph*> LintRunRules(const UBlueprint* Blueprint, TFunctionRef<bool(const TCHAR*)> On, TArray<FFinding>& Findings, int32& OutSkipped)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    Graphs.RemoveAll([Blueprint](UEdGraph* Graph)
    {
        return !Graph || Blueprint->MacroGraphs.Contains(Graph) || Blueprint->DelegateSignatureGraphs.Contains(Graph);
    });
    for (const UEdGraph* Graph : Graphs)
    {
        if (On(TEXT("tick_heavy"))) TickHeavy(Graph, Findings);
        if (On(TEXT("unreachable"))) Unreachable(Graph, Findings);
        if (On(TEXT("random_fanout"))) RandomFanout(Graph, Findings);
        if (On(TEXT("infinite_while"))) InfiniteWhile(Graph, Findings);
    }
    if (On(TEXT("never_assigned")))
    {
        NeverAssigned(Blueprint, Graphs, Findings);
    }
    if (On(TEXT("unguarded_read")))
    {
        UnguardedRead(Blueprint, Graphs, Findings);
    }
    OutSkipped = NumericRules(Blueprint, Graphs, On, Findings);
    return Graphs;
}
} // namespace

bool HandleLint(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    if (Context.SubAction != TEXT("lint_blueprint"))
    {
        return false;
    }
    TSet<FString> Wanted;
    const TArray<TSharedPtr<FJsonValue>>* Asked = nullptr;
    if (Context.Payload->TryGetArrayField(TEXT("rules"), Asked) && Asked)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Asked)
        {
            const FString Rule = Value.IsValid() ? Value->AsString() : FString();
            bool bKnown = false;
            for (const TCHAR* Known : Rules)
            {
                bKnown |= Rule == Known;
            }
            if (!bKnown)
            {
                FString Names;
                for (const TCHAR* Known : Rules)
                {
                    Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(Known);
                }
                Context.SendError(FString::Printf(TEXT("Unknown rule '%s'. Rules: %s."), *Rule, *Names), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            Wanted.Add(Rule);
        }
    }
    double MaxFindings = 50;
    Context.Payload->TryGetNumberField(TEXT("maxFindings"), MaxFindings);
    const int32 Cap = FMath::Clamp(static_cast<int32>(MaxFindings), 1, 200);
    const auto On = [&Wanted](const TCHAR* Rule) { return Wanted.Num() == 0 || Wanted.Contains(Rule); };
    TArray<FFinding> Findings;
    int32 Skipped = 0;
    const TArray<UEdGraph*> Graphs = LintRunRules(Context.Blueprint, On, Findings, Skipped);

    TArray<TSharedPtr<FJsonValue>> Rows;
    TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
    for (const FFinding& Finding : Findings)
    {
        Counts->SetNumberField(Finding.Rule, Counts->HasField(Finding.Rule) ? Counts->GetNumberField(Finding.Rule) + 1 : 1);
        if (Rows.Num() >= Cap)
        {
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("rule"), Finding.Rule);
        Row->SetStringField(TEXT("severity"), Finding.Severity);
        Row->SetStringField(TEXT("graph"), GetNameSafe(Finding.Node->GetGraph()));
        Row->SetStringField(TEXT("node"), Finding.Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        Row->SetStringField(TEXT("nodeId"), Finding.Node->NodeGuid.ToString());
        Row->SetStringField(TEXT("detail"), Finding.Detail);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("findings"), Rows);
    Result->SetObjectField(TEXT("counts"), Counts);
    Result->SetNumberField(TEXT("findingCount"), Findings.Num());
    Result->SetBoolField(TEXT("truncated"), Findings.Num() > Rows.Num());
    Result->SetNumberField(TEXT("graphsChecked"), Graphs.Num());
    const FString Unread = Skipped > 0
        ? FString::Printf(TEXT(" The values of %d event(s) or function(s) were not followed: more paths than the value rules follow."), Skipped)
        : FString();
    if (Skipped > 0)
    {
        Result->SetNumberField(TEXT("valuesSkipped"), Skipped);
    }
    Context.NameBlueprint(Result, false);
    Context.SendResponse((Findings.Num() == 0
        ? FString::Printf(TEXT("No findings in %s (%d graphs)."), *Context.Blueprint->GetName(), Graphs.Num())
        : FString::Printf(TEXT("%d finding(s) in %s."), Findings.Num(), *Context.Blueprint->GetName())) + Unread, Result);
    return true;
}
} // namespace McpBPLint

TArray<FMcpLintIssue> McpLintBlueprint(const UBlueprint* Blueprint)
{
    TArray<FMcpLintIssue> Issues;
    if (!Blueprint)
    {
        return Issues;
    }
    TArray<McpBPLint::FFinding> Findings;
    int32 Skipped = 0;
    McpBPLint::LintRunRules(Blueprint, [](const TCHAR*) { return true; }, Findings, Skipped);
    for (const McpBPLint::FFinding& Finding : Findings)
    {
        Issues.Add({Finding.Rule, Finding.Severity, GetNameSafe(Finding.Node->GetGraph()),
                    Finding.Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), Finding.Detail});
    }
    return Issues;
}
