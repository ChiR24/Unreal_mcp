#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"

namespace McpBlueprintGraphHandlers::GraphBatch
{
namespace
{
constexpr int32 AutoColumns = 5;
// A refused slot moves along the overlap guard's own suggestions (free space right of what it hit) for
// this many hops, then to the auto grid, which is clear of everything the graph held when the batch began.
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
    const bool bAuto = bCreates && !Payload->HasField(TEXT("posX")) && !Payload->HasField(TEXT("x"));
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
}
