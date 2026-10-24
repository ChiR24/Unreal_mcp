#include "Domains/BlueprintGraph/Lint/McpBPLint.h"

#include "Domains/BlueprintGraph/Lint/McpBPLintRules.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

namespace McpBPLint
{
namespace
{
const TCHAR* const Rules[] = {TEXT("tick_heavy"), TEXT("unreachable"), TEXT("random_fanout"), TEXT("infinite_while"),
                              TEXT("never_assigned"), TEXT("unguarded_read")};
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
                Context.SendError(FString::Printf(TEXT("Unknown rule '%s'. Rules: tick_heavy, unreachable, random_fanout, infinite_while, never_assigned, unguarded_read."), *Rule), TEXT("INVALID_ARGUMENT"));
                return true;
            }
            Wanted.Add(Rule);
        }
    }
    double MaxFindings = 50;
    Context.Payload->TryGetNumberField(TEXT("maxFindings"), MaxFindings);
    const int32 Cap = FMath::Clamp(static_cast<int32>(MaxFindings), 1, 200);
    const auto On = [&Wanted](const TCHAR* Rule) { return Wanted.Num() == 0 || Wanted.Contains(Rule); };

    // The graphs that run: event graphs, functions and what they collapse; macro and delegate signatures are not code
    // of this Blueprint.
    TArray<UEdGraph*> Graphs;
    Context.Blueprint->GetAllGraphs(Graphs);
    Graphs.RemoveAll([&Context](UEdGraph* Graph)
    {
        return !Graph || Context.Blueprint->MacroGraphs.Contains(Graph) || Context.Blueprint->DelegateSignatureGraphs.Contains(Graph);
    });
    TArray<FFinding> Findings;
    for (const UEdGraph* Graph : Graphs)
    {
        if (On(TEXT("tick_heavy"))) TickHeavy(Graph, Findings);
        if (On(TEXT("unreachable"))) Unreachable(Graph, Findings);
        if (On(TEXT("random_fanout"))) RandomFanout(Graph, Findings);
        if (On(TEXT("infinite_while"))) InfiniteWhile(Graph, Findings);
    }
    if (On(TEXT("never_assigned")))
    {
        NeverAssigned(Context.Blueprint, Graphs, Findings);
    }
    if (On(TEXT("unguarded_read")))
    {
        UnguardedRead(Context.Blueprint, Graphs, Findings);
    }

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
    Context.NameBlueprint(Result, false);
    Context.SendResponse(Findings.Num() == 0
        ? FString::Printf(TEXT("No findings in %s (%d graphs)."), *Context.Blueprint->GetName(), Graphs.Num())
        : FString::Printf(TEXT("%d finding(s) in %s."), Findings.Num(), *Context.Blueprint->GetName()), Result);
    return true;
}
} // namespace McpBPLint
