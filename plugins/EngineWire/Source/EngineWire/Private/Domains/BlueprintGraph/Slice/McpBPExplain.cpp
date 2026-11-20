// explain_variable (McpBPSlice.h), the Whyline's causal half: the slice's writers and the Branches every path to them
// passes are marked as logpoints (Domains/Debug/Logpoints) for the next Play; read then answers, writer by writer, that it
// ran, or the first Branch on its way that never ran or read the wrong value, with the value it read.

#include "Domains/BlueprintGraph/Slice/McpBPSlice.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Domains/Debug/Logpoints/McpLogpoints.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

namespace McpBPExplainDetail
{
// The node a slice row names: its Blueprint (by package) among Scope, then the node by its id.
UEdGraphNode* NodeOf(const TArray<UBlueprint*>& Scope, const TSharedPtr<FJsonObject>& Row, UBlueprint*& OutBlueprint, const FString& Id)
{
    const FString Package = Row->GetStringField(TEXT("blueprint"));
    for (UBlueprint* Blueprint : Scope)
    {
        if (Blueprint->GetOutermost()->GetName() != Package) continue;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString() == Id)
                {
                    OutBlueprint = Blueprint;
                    return Node;
                }
            }
        }
    }
    return nullptr;
}

const McpLogpoints::FPoint* PointOf(const FString& Id)
{
    return McpLogpoints::Points().FindByPredicate([&Id](const McpLogpoints::FPoint& Point) { return Point.NodeId == Id; });
}

bool Mark(UBlueprint* Blueprint, UEdGraphNode* Node)
{
    TArray<McpLogpoints::FPoint>& All = McpLogpoints::Points();
    if (!Node || PointOf(Node->NodeGuid.ToString()) || All.Num() >= McpLogpoints::MaxPoints) return false;
    McpLogpoints::FPoint& Point = All.AddDefaulted_GetRef();
    Point.Blueprint = Blueprint;
    Point.Node = Node;
    Point.NodeId = Node->NodeGuid.ToString();
    Point.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
    Point.Graph = GetNameSafe(Node->GetGraph());
    return true;
}

// One row's verdict from what the marks recorded: a writer of a variable ("set"), or a call of a function ("called").
TSharedPtr<FJsonObject> Verdict(const TSharedPtr<FJsonObject>& Writer, bool bCall)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    const FString Id = Writer->GetStringField(TEXT("nodeId"));
    const TCHAR* Never = bCall ? TEXT("never called") : TEXT("never set");
    Out->SetStringField(bCall ? TEXT("call") : TEXT("writer"), Writer->GetStringField(TEXT("blueprint")) + TEXT(" ") + Writer->GetStringField(TEXT("node")));
    const McpLogpoints::FPoint* Set = PointOf(Id);
    if (!Set)
    {
        Out->SetStringField(TEXT("verdict"), TEXT("not marked: run op arm, then Play"));
        return Out;
    }
    if (Set->Runs > 0)
    {
        Out->SetStringField(TEXT("verdict"), FString::Printf(TEXT("ran %d time(s), last at %.2f s on %s"), Set->Runs,
            Set->Hits.Num() > 0 ? Set->Hits.Last().GameSeconds : 0.0, Set->Hits.Num() > 0 ? *Set->Hits.Last().ObjectName : TEXT("?")));
        return Out;
    }
    const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
    Writer->TryGetArrayField(TEXT("conditions"), Conditions);
    for (const TSharedPtr<FJsonValue>& Value : Conditions ? *Conditions : TArray<TSharedPtr<FJsonValue>>())
    {
        const TSharedPtr<FJsonObject> Condition = Value->AsObject();
        const McpLogpoints::FPoint* Branch = PointOf(Condition->GetStringField(TEXT("nodeId")));
        const FString Text = Condition->GetStringField(TEXT("condition"));
        if (!Branch || Branch->Runs == 0)
        {
            TArray<FString> Entries;
            const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
            for (const TSharedPtr<FJsonValue>& Entry : Writer->TryGetArrayField(TEXT("entries"), Rows) ? *Rows : TArray<TSharedPtr<FJsonValue>>())
            {
                Entries.Add(Entry->AsObject()->GetStringField(TEXT("node")));
            }
            Out->SetStringField(TEXT("verdict"), FString::Printf(TEXT("%s: the Branch on %s before it never ran, so what leads there (%s) did not happen in that Play"),
                Never, *Text, Entries.Num() > 0 ? *FString::Join(Entries, TEXT(", ")) : TEXT("no entry found")));
            return Out;
        }
        FString Read;
        const TSharedPtr<FJsonObject> Inputs = Branch->Hits.Num() > 0 ? Branch->Hits.Last().Inputs : nullptr;
        if (Inputs.IsValid()) Inputs->TryGetStringField(TEXT("Condition"), Read);
        if (Read.ToBool() != Condition->GetBoolField(TEXT("mustBe")))
        {
            Out->SetStringField(TEXT("verdict"), FString::Printf(TEXT("%s: the Branch on %s ran %d time(s) and read %s the last time; it must be %s to reach the %s"),
                Never, *Text, Branch->Runs, Read.IsEmpty() ? TEXT("?") : *Read, Condition->GetBoolField(TEXT("mustBe")) ? TEXT("true") : TEXT("false"),
                bCall ? TEXT("call") : TEXT("set")));
            return Out;
        }
    }
    Out->SetStringField(TEXT("verdict"), FString::Printf(TEXT("%s, though every Branch before it let it through: a node between them did not continue (a cast that failed, a gate closed)"), Never));
    return Out;
}
} // namespace McpBPExplainDetail

namespace McpBPSlice
{
bool HandleExplain(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    using namespace McpBPExplainDetail;
    // explain_function: the same marks on every call of a function or custom event (slice_function's callers).
    const bool bCall = Context.SubAction == TEXT("explain_function");
    if (!bCall && Context.SubAction != TEXT("explain_variable"))
    {
        return false;
    }
    const FName Variable(*GetJsonStringField(Context.Payload, bCall ? TEXT("function") : TEXT("variable")));
    const FNormalizedAssetPath Folder = NormalizeAssetPath(GetJsonStringField(Context.Payload, TEXT("scope"), TEXT("/Game")));
    int32 Skipped = 0;
    const TArray<UBlueprint*> Blueprints = Folder.bIsValid ? ScopeFor(Context.Blueprint, Folder.Path, 200, Skipped) : TArray<UBlueprint*>{Context.Blueprint};
    const FCallSlice Calls = bCall ? SliceCalls(Context.Blueprint, Variable, Blueprints) : FCallSlice();
    if (!Calls.Error.IsEmpty())
    {
        Context.SendError(Calls.Error, TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }
    const TArray<TSharedPtr<FJsonValue>> Writers = bCall ? Calls.Callers : Slice(Context.Blueprint, Variable, Blueprints)->GetArrayField(TEXT("writers"));
    if (Writers.Num() == 0)
    {
        Context.SendError(bCall
            ? FString::Printf(TEXT("Nothing in scope calls %s (an event the engine, a timer or a bound dispatcher fires is no call here), so there is no call to watch."), *Variable.ToString())
            : FString::Printf(TEXT("Nothing in scope sets %s, so nothing can make it change: slice_variable lists its readers."), *Variable.ToString()),
            bCall ? TEXT("NO_CALLERS") : TEXT("NO_WRITERS"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(bCall ? TEXT("function") : TEXT("variable"), Variable.ToString());
    if (GetJsonStringField(Context.Payload, TEXT("op"), TEXT("arm")) == TEXT("read"))
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const TSharedPtr<FJsonValue>& Writer : Writers) Rows.Add(MakeShared<FJsonValueObject>(Verdict(Writer->AsObject(), bCall)));
        Result->SetArrayField(bCall ? TEXT("callers") : TEXT("writers"), Rows);
        Context.NameBlueprint(Result, false);
        Context.SendResponse(Rows[0]->AsObject()->GetStringField(TEXT("verdict")), Result);
        return true;
    }
    int32 Marked = 0;
    for (const TSharedPtr<FJsonValue>& Value : Writers)
    {
        const TSharedPtr<FJsonObject> Writer = Value->AsObject();
        UBlueprint* Owner = nullptr;
        UEdGraphNode* Set = NodeOf(Blueprints, Writer, Owner, Writer->GetStringField(TEXT("nodeId")));
        Marked += Mark(Owner, Set) ? 1 : 0;
        const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
        for (const TSharedPtr<FJsonValue>& Condition : Writer->TryGetArrayField(TEXT("conditions"), Conditions) ? *Conditions : TArray<TSharedPtr<FJsonValue>>())
        {
            UEdGraphNode* Branch = NodeOf(Blueprints, Writer, Owner, Condition->AsObject()->GetStringField(TEXT("nodeId")));
            Marked += Mark(Owner, Branch) ? 1 : 0;
        }
    }
    McpLogpoints::Listen(true);
    Result->SetNumberField(TEXT("marked"), Marked);
    Result->SetNumberField(bCall ? TEXT("callerCount") : TEXT("writerCount"), Writers.Num());
    Context.NameBlueprint(Result, false);
    Context.SendResponse(FString::Printf(TEXT("Marked %d node(s) on the way to %d %s of %s. Play, do what should %s it, then call again with op read."),
        Marked, Writers.Num(), bCall ? TEXT("call(s)") : TEXT("set(s)"), *Variable.ToString(), bCall ? TEXT("call") : TEXT("set")), Result);
    return true;
}
} // namespace McpBPSlice
