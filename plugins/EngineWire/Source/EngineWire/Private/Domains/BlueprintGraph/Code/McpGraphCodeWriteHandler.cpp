#include "Domains/BlueprintGraph/Code/McpGraphCodeWrite.h"

#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPDiag.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGraphCode
{
namespace WriteHandlerParts
{
// The compiler's messages on the nodes the write made, by the line that made each.
TArray<TSharedPtr<FJsonValue>> MessagesByLine(const TMap<UEdGraphNode*, int32>& Made)
{
    TArray<TPair<int32, FString>> Found;
    for (const TPair<UEdGraphNode*, int32>& Each : Made)
    {
        if (Each.Key->bHasCompilerMessage && Each.Key->ErrorType <= EMessageSeverity::Warning)
        {
            // A node's message holds a line per compiler message, often the same one twice.
            const TCHAR* Severity = Each.Key->ErrorType <= EMessageSeverity::Error ? TEXT("error") : TEXT("warning");
            TArray<FString> Lines;
            Each.Key->ErrorMsg.ParseIntoArrayLines(Lines);
            for (const FString& Text : Lines)
            {
                Found.AddUnique(TPair<int32, FString>(Each.Value, FString(Severity) + TEXT(": ") + Text));
            }
        }
    }
    Found.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B)
               { return A.Key != B.Key ? A.Key < B.Key : A.Value.Compare(B.Value, ESearchCase::CaseSensitive) < 0; });
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const TPair<int32, FString>& Each : Found)
    {
        Rows.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("line %d %s"), Each.Key, *Each.Value)));
    }
    return Rows;
}

// The written graphs read back, as read_graph_as_code prints them.
FString ReadBack(UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs)
{
    TArray<FGraph> Reads;
    for (UEdGraph* Graph : Graphs)
    {
        Reads.Add(Snapshot(Graph));
    }
    ShortenIds(Reads);
    ExpandTypeIds(Blueprint, Graphs, Reads);
    TArray<FString> Code;
    TArray<FString> Warnings;
    for (int32 At = 0; At < Graphs.Num(); ++At)
    {
        TSet<int32> Placed;
        Code.Add(TEXT("; graph ") + Graphs[At]->GetName());
        Code.Add(Decompile(Reads[At], Placed, Warnings));
    }
    return FString::Join(Code, TEXT("\n"));
}

void Rollback(UBlueprint* Blueprint, const McpBlueprintBehaviour::FSnapshot& Before)
{
    TArray<TSharedPtr<FJsonValue>> Results;
    McpBlueprintBehaviour::UndoBatch(Blueprint, Before, TArray<TSharedPtr<FJsonValue>>(), Results);
}

// The graph each form goes to: an (event) to graphName's event graph (the first by default), an (fn) to its function.
bool RouteForms(const McpBlueprintGraphHandlers::FActionContext& Context, const TArray<FSExpr>& Forms, TArray<Write::FGraphPlan>& Plans)
{
    using namespace Write;
    UBlueprint* Blueprint = Context.Blueprint;
    for (int32 At = 0; At < Forms.Num(); ++At)
    {
        const FString Error = CheckForm(Forms[At]);
        if (!Error.IsEmpty())
        {
            Context.SendError(Error, TEXT("CODE_INVALID"));
            return false;
        }
        const FSExpr& Form = Forms[At];
        const bool bFunction = Form.Items[0].Text == TEXT("fn");
        UEdGraph* Graph = bFunction ? FindFunction(Blueprint, FormName(Form)) : Context.TargetGraph;
        if (bFunction && !Graph)
        {
            Context.SendError(FString::Printf(TEXT("Line %d: %s has no function %s: add it (manage_blueprint add_function), then write its body."),
                                              Form.Line, *Blueprint->GetName(), *FormName(Form)),
                              TEXT("FUNCTION_NOT_FOUND"));
            return false;
        }
        if (!bFunction && !(Graph && Blueprint->UbergraphPages.Contains(Graph)))
        {
            Context.SendError(FString::Printf(TEXT("Line %d: (event %s) goes into an event graph, and %s has none by that name; graphName names "
                                                   "the event graph (the first by default)."),
                                              Form.Line, *FormName(Form), *Blueprint->GetName()),
                              TEXT("GRAPH_NOT_FOUND"));
            return false;
        }
        FGraphPlan* Plan = Plans.FindByPredicate([Graph](const FGraphPlan& Each) { return Each.Graph == Graph; });
        Plan = Plan ? Plan : &Plans.AddDefaulted_GetRef();
        Plan->Graph = Graph;
        Plan->Forms.Add(At);
    }
    return true;
}
} // namespace WriteHandlerParts

using namespace WriteHandlerParts;

bool HandleWriteGraphFromCode(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    using namespace Write;
    if (Context.SubAction != TEXT("write_graph_from_code"))
    {
        return false;
    }
    if (GEditor && GEditor->IsPlaySessionInProgress())
    {
        Context.SendError(TEXT("Stop Play In Editor first: compiling a Blueprint during PIE reinstances the running game."), TEXT("PIE_ACTIVE"));
        return true;
    }
    UBlueprint* Blueprint = Context.Blueprint;
    FString Code;
    FString Error;
    TArray<FSExpr> Forms;
    Context.Payload->TryGetStringField(TEXT("code"), Code);
    if (!ParseSExpr(Code, Forms, Error) || Forms.Num() == 0)
    {
        Context.SendError(Error.IsEmpty() ? TEXT("code holds no form: write (event Name ...) or (fn Name (params) ...).") : Error, TEXT("CODE_PARSE_ERROR"));
        return true;
    }
    TArray<FGraphPlan> Plans;
    if (!RouteForms(Context, Forms, Plans))
    {
        return true;
    }
    for (FGraphPlan& Plan : Plans)
    {
        if (!PlanGraph(Forms, Plan, Error))
        {
            Context.SendError(Error, TEXT("CODE_INVALID"));
            return true;
        }
    }
    const TSet<FString> Titles = CodeTitles(Forms);
    const McpBlueprintBehaviour::FSnapshot Before = McpBlueprintBehaviour::TakeSnapshot(Blueprint);
    // After the snapshot: a dirty Blueprint compiles there, and spawners gathered before name its old skeleton's members.
    const TArray<FMenuEntry> Candidates = MenuCandidates(Titles);
    TMap<UEdGraphNode*, int32> Made;
    TArray<UEdGraph*> Graphs;
    TArray<TSharedPtr<FJsonValue>> FormRows;
    for (FGraphPlan& Plan : Plans)
    {
        if (!BuildForms(Blueprint, Forms, Candidates, Titles, Plan, Made, FormRows, Error))
        {
            Rollback(Blueprint, Before);
            Context.SendError(Error + TEXT(" Nothing was written."), TEXT("CODE_INVALID"));
            return true;
        }
        Graphs.Add(Plan.Graph);
        // A function keeps its old return nodes when the code makes none: they hold its return values.
        bool bReturns = false;
        for (const TPair<UEdGraphNode*, int32>& Each : Made)
        {
            bReturns = bReturns || (Each.Key->IsA<UK2Node_FunctionResult>() && Each.Key->GetGraph() == Plan.Graph);
        }
        Plan.Old.RemoveAll([bReturns](const UEdGraphNode* Node) { return !bReturns && Node->IsA<UK2Node_FunctionResult>(); });
    }
    // The old bodies sit out the compile (out of their graphs, links cut) and go only once the new code compiles, so a
    // refusal can put every node back.
    for (FGraphPlan& Plan : Plans)
    {
        for (UEdGraphNode* Node : Plan.Old)
        {
            Plan.Graph->RemoveNode(Node);
        }
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    FString FirstError;
    const bool bCompiled = McpCompileBlueprintWithDiagnostics(Blueprint, Result, FirstError, 12);
    const TArray<TSharedPtr<FJsonValue>> Messages = MessagesByLine(Made);
    for (FGraphPlan& Plan : Plans)
    {
        for (UEdGraphNode* Node : Plan.Old)
        {
            Plan.Graph->AddNode(Node, false, false);
        }
    }
    if (!bCompiled && Before.bCompiled)
    {
        Rollback(Blueprint, Before);
        Result->SetArrayField(TEXT("compileMessages"), Messages);
        Context.SendErrorWithDetails(FString::Printf(TEXT("Nothing was written: the Blueprint compiled before and would not with this code: %s"),
                                                     FirstError.IsEmpty() ? TEXT("no compiler message") : *FirstError),
                                     TEXT("CODE_BREAKS_COMPILE"), Result);
        return true;
    }
    int32 Removed = 0;
    int32 Kept = 0;
    for (FGraphPlan& Plan : Plans)
    {
        Kept += Plan.Kept;
        for (UEdGraphNode* Node : Plan.Old)
        {
            FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile=*/true);
            ++Removed;
        }
    }
    if (Messages.Num() > 0)
    {
        Result->SetArrayField(TEXT("compileMessages"), Messages);
    }
    Result->SetStringField(TEXT("code"), ReadBack(Blueprint, Graphs));
    Result->SetArrayField(TEXT("forms"), FormRows);
    Result->SetNumberField(TEXT("created"), Made.Num() - Kept);
    Result->SetNumberField(TEXT("kept"), Kept);
    Result->SetNumberField(TEXT("removed"), Removed);
    Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Blueprint));
    Context.NameBlueprint(Result, true);
    // A Blueprint that did not compile before keeps the write: its errors may lie elsewhere.
    const FString StillBroken = bCompiled ? FString() : FString::Printf(TEXT(" It still does not compile (it did not before either): %s"), *FirstError);
    Context.SendResponse(FString::Printf(TEXT("Wrote %d form(s) into %s: %d node(s) made, %d kept, %d removed.%s"), Forms.Num(),
                                         *Blueprint->GetName(), Made.Num() - Kept, Kept, Removed, *StillBroken),
                         Result);
    return true;
}
} // namespace McpGraphCode
