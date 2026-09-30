#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"

#include "EdGraphSchema_K2.h"

namespace McpBlueprintGraphHandlers::GraphBatch
{
namespace
{
constexpr int32 AutoColumns = 5;
// A refused slot moves to the overlap guard's own suggestion (the nearest free slot, else free space right
// of what it hit) for this many hops, then to the auto grid, clear of everything the graph held when the batch began.
constexpr int32 MaxSuggestionHops = 24;
constexpr int32 MaxPlacementTries = 48;

// Auto-placed nodes fill a grid right of whatever the graph already holds.
void PlaceOnGrid(FBatchState& State, const TSharedPtr<FJsonObject>& Payload)
{
    const int32 Slot = State.AutoPlaced++;
    Payload->SetNumberField(TEXT("posX"), State.OriginX + (Slot % AutoColumns) * 360.0f);
    Payload->SetNumberField(TEXT("posY"), (Slot / AutoColumns) * 260.0f);
}

bool IsOverlapRefusal(const FMcpCapturedResponse& Reply, const TSharedPtr<FJsonObject>*& OutSuggested)
{
    return !Reply.bSuccess && Reply.ErrorCode == TEXT("NODE_OVERLAP") && Reply.Result.IsValid() &&
           Reply.Result->TryGetObjectField(TEXT("suggestedPosition"), OutSuggested);
}

// The titles of the nodes a refused slot overlapped, for the step's placementWarning.
FString OverlappedTitles(const FMcpCapturedResponse& Refusal)
{
    TArray<FString> Titles;
    const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
    if (Refusal.Result->TryGetArrayField(TEXT("overlappingNodes"), Nodes))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Nodes)
        {
            const TSharedPtr<FJsonObject>* Node = nullptr;
            FString Title;
            if (Value.IsValid() && Value->TryGetObject(Node) && (*Node)->TryGetStringField(TEXT("title"), Title))
            {
                Titles.AddUnique(Title);
            }
        }
    }
    return Titles.Num() > 0 ? FString::Join(Titles, TEXT(", ")) : FString(TEXT("an existing node"));
}

// Where a node goes beside one it is wired to; the lowest Rank wins. bBetterPending: a neighbour
// that would rank better has not settled yet.
struct FAnchor
{
    int32 Rank = MAX_int32;
    float X = 0.0f;
    float Y = 0.0f;
    bool bBetterPending = false;
};

// Right of what runs it, below-left of what reads it, left of what it runs, right of what feeds it.
// A node still waiting to be settled is no anchor: it sits on the grid.
FAnchor AnchorBeside(const UEdGraphNode& Node, const TSet<const UEdGraphNode*>& Unsettled)
{
    constexpr float Gap = 80.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    McpGraphLayout::EstimateNodeExtent(Node, Width, Height);
    FAnchor Best;
    int32 PendingRank = MAX_int32;
    for (const UEdGraphPin* Pin : Node.Pins)
    {
        if (!Pin)
        {
            continue;
        }
        const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
        const bool bInput = Pin->Direction == EGPD_Input;
        for (const UEdGraphPin* Linked : Pin->LinkedTo)
        {
            const UEdGraphNode* Other = Linked ? Linked->GetOwningNode() : nullptr;
            const int32 Rank = bExec ? (bInput ? 0 : 2) : (bInput ? 3 : 1);
            if (Other && Unsettled.Contains(Other))
            {
                PendingRank = FMath::Min(PendingRank, Rank);
                continue;
            }
            if (!Other || Rank >= Best.Rank)
            {
                continue;
            }
            float OtherW = 0.0f;
            float OtherH = 0.0f;
            McpGraphLayout::EstimateNodeExtent(*Other, OtherW, OtherH);
            Best.Rank = Rank;
            Best.X = Rank == 0 || Rank == 3 ? Other->NodePosX + OtherW + Gap : Other->NodePosX - Width - Gap;
            Best.Y = Rank == 1 ? Other->NodePosY + OtherH + Gap : Other->NodePosY;
        }
    }
    Best.bBetterPending = PendingRank < Best.Rank;
    return Best;
}
} // namespace

FMcpCapturedResponse RunStep(const FActionContext& Parent, const TSharedPtr<FJsonObject>& Payload,
                             const FString& Edit, const FString& StepId, FString& OutPins)
{
    FMcpResponseCaptureRegistry::Get().Begin(StepId);
    FActionContext Step{Parent.Subsystem, StepId, Payload, Parent.RequestingSocket, Edit};
    Step.bDeferCompile = true;
    if (!RunBlueprintMemberStep(Parent, Edit, StepId, Payload) && PrepareBlueprintAndGraph(Step))
    {
        const bool bHandled = HandleNodeCreationAction(Step) || HandlePinMutationAction(Step) ||
                              HandleNodeMutationAction(Step);
        (void)bHandled;
    }
    FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(StepId);
    FString Guid;
    if (Reply.bSuccess && Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("nodeGuid"), Guid))
    {
        // A member step (add_event: its parameter pins are what later steps wire) prepares no
        // graph, so its node is found across the Blueprint; it used to report no pins at all.
        OutPins = DescribeNodePins(Step.TargetGraph ? Step.FindNode(Guid) : FindBatchNode(Parent.Blueprint, Guid));
    }
    return Reply;
}

// A create step that names no position is auto-placed. A slot that turns out to be taken, auto or
// caller-chosen, is never a reason to fail the batch: the node moves to the overlap guard's own
// suggestion, hop by hop (a row of nodes can take a dozen), and to the auto grid if those run out.
// A caller who named the position is told where the node went, in the step's placementWarning.
FMcpCapturedResponse RunPlacedStep(const FActionContext& Parent, FBatchState& State,
                                   const TSharedPtr<FJsonObject>& Payload, const FString& Edit,
                                   const FString& StepId, FString& OutPins)
{
    const bool bCreates = Edit == TEXT("create_node") || Edit == TEXT("create_reroute_node");
    // add_event reads posX/posY too: without them its node landed on the graph origin, over whatever sat there.
    const bool bAuto = (bCreates || Edit == TEXT("add_event")) && !Payload->HasField(TEXT("posX")) &&
                       !Payload->HasField(TEXT("x"));
    if (bAuto)
    {
        PlaceOnGrid(State, Payload);
    }
    // The handlers read `x`/`y` before `posX`/`posY`; move them over so a move below
    // (which writes posX/posY) takes effect.
    double Coord = 0.0;
    if (Payload->TryGetNumberField(TEXT("x"), Coord))
    {
        Payload->SetNumberField(TEXT("posX"), Coord);
        Payload->RemoveField(TEXT("x"));
    }
    if (Payload->TryGetNumberField(TEXT("y"), Coord))
    {
        Payload->SetNumberField(TEXT("posY"), Coord);
        Payload->RemoveField(TEXT("y"));
    }
    double RequestedX = 0.0;
    double RequestedY = 0.0;
    Payload->TryGetNumberField(TEXT("posX"), RequestedX);
    Payload->TryGetNumberField(TEXT("posY"), RequestedY);
    FMcpCapturedResponse Reply = RunStep(Parent, Payload, Edit, StepId, OutPins);
    const TSharedPtr<FJsonObject>* Suggested = nullptr;
    FString Overlapped;
    int32 Moves = 0;
    for (; bCreates && Moves < MaxPlacementTries && IsOverlapRefusal(Reply, Suggested); ++Moves)
    {
        if (Moves == 0)
        {
            Overlapped = OverlappedTitles(Reply);
        }
        if (Moves < MaxSuggestionHops)
        {
            Payload->SetNumberField(TEXT("posX"), (*Suggested)->GetNumberField(TEXT("x")));
            Payload->SetNumberField(TEXT("posY"), (*Suggested)->GetNumberField(TEXT("y")));
        }
        else
        {
            PlaceOnGrid(State, Payload);
        }
        Reply = RunStep(Parent, Payload, Edit, StepId, OutPins);
    }
    FString CreatedGuid;
    if (bAuto && Reply.bSuccess && Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("nodeGuid"), CreatedGuid))
    {
        State.AutoPlacedGuids.Add(CreatedGuid);
    }
    if (Moves > 0 && !bAuto && Reply.bSuccess && Reply.Result.IsValid())
    {
        double PlacedX = RequestedX;
        double PlacedY = RequestedY;
        Payload->TryGetNumberField(TEXT("posX"), PlacedX);
        Payload->TryGetNumberField(TEXT("posY"), PlacedY);
        FString Warning = FString::Printf(
            TEXT("Requested position (%d, %d) overlaps %s; the node was placed at (%d, %d) instead."),
            FMath::RoundToInt(RequestedX), FMath::RoundToInt(RequestedY), *Overlapped,
            FMath::RoundToInt(PlacedX), FMath::RoundToInt(PlacedY));
        FString Existing;
        if (Reply.Result->TryGetStringField(TEXT("placementWarning"), Existing) && !Existing.IsEmpty())
        {
            Warning += TEXT(" ") + Existing;
        }
        Reply.Result->SetStringField(TEXT("placementWarning"), Warning);
    }
    return Reply;
}

// The grid sits right of everything the graph held, so a chain added to a big graph landed thousands of
// units from the event it hangs off (a Bounce event at x 7646 wired to a new node at x 46464). Each
// auto-placed node now moves beside a node it is wired to, the nearest free slot when that spot is taken;
// one wired only to other new nodes follows them once they have moved, and one with no free slot stays.
TArray<UEdGraphNode*> SettleAutoPlacedNodes(UBlueprint* Blueprint, const FBatchState& State)
{
    TArray<UEdGraphNode*> Pending;
    TSet<const UEdGraphNode*> Unsettled;
    for (const FString& Guid : State.AutoPlacedGuids)
    {
        if (UEdGraphNode* Node = FindBatchNode(Blueprint, Guid))
        {
            Pending.Add(Node);
            Unsettled.Add(Node);
        }
    }
    // A node waits while the neighbour it belongs beside has not settled: a pure node went beside the
    // far-left variable it reads (x 8494) when the Branch reading it settled later (x 10134). A pass that
    // settles nothing lets one node take its best settled neighbour, then the waiting resumes.
    for (bool bRelax = false;;)
    {
        bool bMoved = false;
        for (int32 Index = 0; Index < Pending.Num() && !(bRelax && bMoved); ++Index)
        {
            UEdGraphNode* Node = Pending[Index];
            const FAnchor Anchor = AnchorBeside(*Node, Unsettled);
            if (Anchor.bBetterPending && !bRelax)
            {
                continue;
            }
            float Width = 0.0f;
            float Height = 0.0f;
            McpGraphLayout::EstimateNodeExtent(*Node, Width, Height);
            float X = Anchor.X;
            float Y = Anchor.Y;
            // A taken spot stays on the anchor's side: right of what runs or feeds the node, left of what
            // reads it or what it runs.
            const int32 PreferDX = Anchor.Rank == 0 || Anchor.Rank == 3 ? 1 : -1;
            TArray<McpGraphLayout::FGraphNodeOccupant> Occupants;
            if (Anchor.Rank == MAX_int32 ||
                (McpGraphLayout::CheckGraphNodeOverlap(Node->GetGraph(), X, Y, Width, Height, Occupants,
                                                       McpGraphLayout::NodeOverlapPadding, Node) &&
                 !McpGraphLayout::FindNearestFreeSlot(Node->GetGraph(), Anchor.X, Anchor.Y, Width, Height, Node, X, Y, PreferDX)))
            {
                continue;
            }
            Node->NodePosX = FMath::RoundToInt(X);
            Node->NodePosY = FMath::RoundToInt(Y);
            Unsettled.Remove(Node);
            Pending.RemoveAt(Index--);
            bMoved = true;
        }
        if (!bMoved && bRelax)
        {
            break;
        }
        bRelax = !bMoved;
    }
    return Pending;
}
}
