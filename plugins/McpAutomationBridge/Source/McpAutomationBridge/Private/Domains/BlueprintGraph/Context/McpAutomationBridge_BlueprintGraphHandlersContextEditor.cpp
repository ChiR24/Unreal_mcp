#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpBlueprintGraphHandlers
{

UEdGraphNode* FActionContext::FindNode(const FString& Id) const
{
    if (Id.IsEmpty())
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (Node &&
            (Node->NodeGuid.ToString().Equals(Id, ESearchCase::IgnoreCase) ||
             Node->GetName().Equals(Id, ESearchCase::IgnoreCase)))
        {
            return Node;
        }
    }

    // QoL: callers often quote a shortened GUID. Accept an UNAMBIGUOUS
    // GUID-prefix of at least 8 hex chars; anything ambiguous stays
    // not-found (the full 32-hex id is always available via
    // get_graph_details). Non-hex ids keep exact-name-only semantics.
    if (Id.Len() >= 8 && Id.Len() < 32)
    {
        bool bHexLike = true;
        for (const TCHAR C : Id)
        {
            if (!FChar::IsHexDigit(C))
            {
                bHexLike = false;
                break;
            }
        }
        if (bHexLike)
        {
            UEdGraphNode* UniqueMatch = nullptr;
            for (UEdGraphNode* Node : TargetGraph->Nodes)
            {
                if (Node && Node->NodeGuid.ToString().StartsWith(Id, ESearchCase::IgnoreCase))
                {
                    if (UniqueMatch)
                    {
                        return nullptr;
                    }
                    UniqueMatch = Node;
                }
            }
            return UniqueMatch;
        }
    }
    return nullptr;
}

UEdGraphPin* FActionContext::FindPin(
    UEdGraphNode* Node,
    const FString& PinName) const
{
    if (!Node || PinName.IsEmpty())
    {
        return nullptr;
    }

    FString CleanPinName;
    if (!PinName.Split(TEXT("."), nullptr, &CleanPinName))
    {
        CleanPinName = PinName;
    }

    auto Squash = [](FString S) { S.ReplaceInline(TEXT(" "), TEXT("")); S.ReplaceInline(TEXT("_"), TEXT("")); return S; };
    auto MatchPin = [Node, &Squash](const FString& Candidate) -> UEdGraphPin*
    {
        if (UEdGraphPin* Pin = Node->FindPin(*Candidate))
        {
            return Pin;
        }
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin &&
                (Pin->PinName.ToString().Equals(
                     Candidate,
                     ESearchCase::IgnoreCase) ||
                 Pin->GetDisplayName().ToString().Equals(
                     Candidate,
                     ESearchCase::IgnoreCase)))
            {
                return Pin;
            }
        }
        // A cast's output is "AsPrimitive Component": a name that drops its spaces or underscores still matches.
        const FString Squashed = Squash(Candidate);
        for (UEdGraphPin* Pin : Node->Pins)
            if (Pin && (Squash(Pin->PinName.ToString()).Equals(Squashed, ESearchCase::IgnoreCase) ||
                        Squash(Pin->GetDisplayName().ToString()).Equals(Squashed, ESearchCase::IgnoreCase))) return Pin;
        return nullptr;
    };

    if (UEdGraphPin* Pin = MatchPin(CleanPinName))
    {
        return Pin;
    }

    FString UnderscorePinName = CleanPinName;
    UnderscorePinName.ReplaceCharInline(TEXT(' '), TEXT('_'));
    if (!UnderscorePinName.Equals(
            CleanPinName,
            ESearchCase::CaseSensitive))
    {
        return MatchPin(UnderscorePinName);
    }
    // A latent or macro node names its own exec input (MoveComponentTo:
    // Move/Stop/Return, ForEachLoop: Exec), so a wire into "execute" missed
    // although the intent is plain. Take the node's first exec input.
    if (CleanPinName.Equals(TEXT("execute"), ESearchCase::IgnoreCase))
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input &&
                Pin->PinType.PinCategory == FName(TEXT("exec")))
            {
                return Pin;
            }
        }
    }
    return nullptr;
}

static UEdGraph* FindTargetGraph(
    UBlueprint* Blueprint,
    const FString& GraphName)
{
    UEdGraph* TargetGraph = nullptr;
    if (GraphName.IsEmpty() ||
        GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            TargetGraph = Blueprint->UbergraphPages[0];
        }
    }
    else
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : Blueprint->UbergraphPages)
            {
                if (Graph->GetName() == GraphName)
                {
                    TargetGraph = Graph;
                    break;
                }
            }
        }
    }

    if (!TargetGraph)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }
    return TargetGraph;
}

// A bare "not found" named nothing that would work. List the graphs, and catch
// the usual miss: the name is an event, which lives INSIDE an event graph.
static FString DescribeMissingGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    TArray<UEdGraph*> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    TArray<FString> Names;
    for (UEdGraph* Graph : AllGraphs)
    {
        Names.AddUnique(Graph->GetName());
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_Event* Event = Cast<UK2Node_Event>(Node);
            if (Event && Event->GetFunctionName().ToString().Equals(GraphName, ESearchCase::IgnoreCase))
            {
                return FString::Printf(TEXT("'%s' is an event inside graph '%s', not a graph: pass graphName '%s' (inspect_graph filter '%s' finds the event)."),
                    *GraphName, *Graph->GetName(), *Graph->GetName(), *Event->GetNodeTitle(ENodeTitleType::ListView).ToString());
            }
        }
    }
    return FString::Printf(TEXT("Could not find graph '%s' in blueprint. Its graphs: %s."), *GraphName, *FString::Join(Names, TEXT(", ")));
}

bool PrepareBlueprintAndGraph(FActionContext& Context)
{
    // blueprintPath is the declared name, so it wins over the legacy assetPath.
    FString AssetPath = McpGetFirstStringField(Context.Payload, {TEXT("blueprintPath"), TEXT("assetPath")});

    const FString AssetPathAsGiven = AssetPath;
    AssetPath = SanitizeProjectRelativePath(AssetPath);
    if (AssetPath.IsEmpty())
    {
        Context.SendError(
            McpPathRefusalMessage(TEXT("asset path"), AssetPathAsGiven),
            TEXT("INVALID_PATH"));
        return false;
    }

    FString NormalizedPath;
    FString LoadError;
    Context.Blueprint =
        LoadBlueprintAsset(AssetPath, NormalizedPath, LoadError);
    if (!Context.Blueprint)
    {
        Context.SendError(
            LoadError.IsEmpty()
                ? FString::Printf(
                      TEXT("Could not load blueprint at path: %s"),
                      *AssetPath)
                : LoadError,
            TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    FString GraphName;
    Context.Payload->TryGetStringField(TEXT("graphName"), GraphName);
    Context.TargetGraph = FindTargetGraph(Context.Blueprint, GraphName);
    if (!Context.TargetGraph)
    {
        Context.SendError(DescribeMissingGraph(Context.Blueprint, GraphName), TEXT("GRAPH_NOT_FOUND"));
        return false;
    }
    return true;
}

}