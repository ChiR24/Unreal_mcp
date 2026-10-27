// create_node kind BindEvent: a "Bind Event to <dispatcher>" node and a custom event with the dispatcher's inputs,
// its delegate pin wired to the bind node's Event pin, as the editor makes them. Without it nothing over MCP could
// make one actor react to another's event dispatcher (a switch opening a door): CallDelegate only fires one.
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviourNodes.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace McpBlueprintGraphHandlers
{
bool IsBindEventKind(const FString& NodeType)
{
    return NodeType.Equals(TEXT("BindEvent"), ESearchCase::IgnoreCase) ||
           NodeType.Equals(TEXT("AddDelegate"), ESearchCase::IgnoreCase) ||
           NodeType.Equals(TEXT("K2Node_AddDelegate"), ESearchCase::IgnoreCase);
}

namespace
{
// Below the bind node, moved further down past anything already there.
FVector2D PlaceBoundEvent(UEdGraph* Graph, const UEdGraphNode& Bind, float X, float Y)
{
    float Width = 0.0f;
    float Height = 0.0f;
    McpGraphLayout::EstimateNodeExtent(Bind, Width, Height);
    float EventY = Y + Height + McpGraphLayout::NodeSuggestGap;
    TArray<McpGraphLayout::FGraphNodeOccupant> Overlapping;
    for (int32 Step = 0; Step < 8 && McpGraphLayout::CheckGraphNodeOverlap(Graph, X, EventY, Width, Height, Overlapping,
                                                                           McpGraphLayout::NodeOverlapPadding, nullptr);
         ++Step)
    {
        for (const McpGraphLayout::FGraphNodeOccupant& Occupant : Overlapping)
        {
            EventY = FMath::Max(EventY, Occupant.Y + Occupant.Height + McpGraphLayout::NodeSuggestGap);
        }
    }
    return FVector2D(X, EventY);
}
} // namespace

bool CreateBindEventNode(FActionContext& Context, float X, float Y)
{
    const FString Member = GetJsonStringField(Context.Payload, TEXT("memberName"));
    const FString MemberClass = McpGetFirstStringField(Context.Payload, {TEXT("memberClass"), TEXT("targetClass")});
    UClass* Owner = MemberClass.IsEmpty() ? nullptr : ResolveBehaviourClass(MemberClass);
    FMulticastDelegateProperty* Dispatcher = FindBehaviourDispatcher(Context.Blueprint, Member, Owner);
    if (!Dispatcher || (!MemberClass.IsEmpty() && !Owner))
    {
        Context.SendError(FString::Printf(TEXT("No event dispatcher '%s' on %s. Name another class's in memberClass (a "
                                               "Blueprint path), or declare this Blueprint's first (add_event_dispatcher)."),
                                          *Member, MemberClass.IsEmpty() ? TEXT("this Blueprint") : *MemberClass),
                          TEXT("DISPATCHER_NOT_FOUND"));
        return true;
    }
    if (!Context.Blueprint->UbergraphPages.Contains(Context.TargetGraph))
    {
        Context.SendError(TEXT("A Bind Event node and the custom event it binds live in an event graph (EventGraph); "
                               "a function graph cannot hold the event."),
                          TEXT("EVENT_NOT_ALLOWED_IN_GRAPH"));
        return true;
    }
    FString EventName = McpGetFirstStringField(Context.Payload, {TEXT("eventName"), TEXT("customEventName")});
    EventName = EventName.IsEmpty() ? Dispatcher->GetName() + TEXT("_Event") : EventName;
    // A name in use takes the next free one (an event removed moments ago stays in the skeleton class until a
    // compile); the reply names the event, and a batch reaches it by "$id_event", not by name.
    if (FKismetNameValidator(Context.Blueprint).IsValid(EventName) != EValidatorResult::Ok)
    {
        EventName = FBlueprintEditorUtils::FindUniqueKismetName(Context.Blueprint, EventName).ToString();
    }

    FGraphNodeCreator<UK2Node_AddDelegate> Creator(*Context.TargetGraph);
    UK2Node_AddDelegate* Bind = Creator.CreateNode(false);
    Bind->SetFromProperty(Dispatcher, /*bSelfContext=*/Owner == nullptr, Dispatcher->GetOwnerClass());
    Bind->NodePosX = X;
    Bind->NodePosY = Y;
    Creator.Finalize();
    FString OverlapMessage;
    TSharedPtr<FJsonObject> OverlapDetails;
    if (McpGraphLayout::RefuseOverlappingNode(Context.TargetGraph, Bind, X, Y, OverlapMessage, OverlapDetails,
                                              McpGraphLayout::NamesNoPosition(Context.Payload)))
    {
        Context.SendErrorWithDetails(OverlapMessage, TEXT("NODE_OVERLAP"), OverlapDetails);
        return true;
    }
    // Under the bind node where it ended up: a call that named no position moves it off what it overlapped.
    UK2Node_CustomEvent* Event = UK2Node_CustomEvent::CreateFromFunction(
        PlaceBoundEvent(Context.TargetGraph, *Bind, Bind->NodePosX, Y), Context.TargetGraph, EventName,
        Dispatcher->SignatureFunction, /*bSelectNewNode=*/false);
    UEdGraphPin* EventDelegate = Event ? Event->FindPin(UK2Node_Event::DelegateOutputName) : nullptr;
    UEdGraphPin* BindDelegate = Bind->GetDelegatePin();
    if (!EventDelegate || !BindDelegate || !Context.TargetGraph->GetSchema()->TryCreateConnection(EventDelegate, BindDelegate))
    {
        if (Event)
        {
            FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Event, /*bDontRecompile=*/true);
        }
        FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Bind, /*bDontRecompile=*/true);
        Context.SendError(FString::Printf(TEXT("A custom event with the inputs of '%s' could not be wired to its Bind "
                                               "Event node."), *Dispatcher->GetName()),
                          TEXT("CREATE_FAILED"));
        return true;
    }
    // The event is a new function on the class: regenerate the skeleton, as a custom event step does.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeGuid"), Bind->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeId"), Bind->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), Bind->GetName());
    Result->SetStringField(TEXT("eventNodeGuid"), Event->NodeGuid.ToString());
    Result->SetStringField(TEXT("eventName"), EventName);
    McpGraphLayout::AddNodePlacementFields(Result, *Bind);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(FString::Printf(TEXT("Bind Event to %s created, wired to custom event %s (its then pin runs "
                                              "when the dispatcher fires); connect the object to bind to its Target pin."),
                                         *Dispatcher->GetName(), *EventName),
                         Result);
    return true;
}
} // namespace McpBlueprintGraphHandlers
