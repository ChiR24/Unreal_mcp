// Hooks: behaviour logic on an event that other code may already use (BeginPlay,
// Tick, AnyDamage, a component overlap, a shared custom event). Every hooked event
// gets ONE shared Sequence node, the hub (comment HubTag), and each behaviour takes
// a free then_N pin of it. Nobody steals the event's exec link - not from the user,
// not from another behaviour - and a replace frees exactly the pins its own nodes used.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintBehaviour::Detail
{
namespace
{
bool IsEventHub(const UEdGraphNode* Node)
{
    return Node && Node->GetClass() == UK2Node_ExecutionSequence::StaticClass() && Node->NodeComment == HubTag;
}

// Event.then -> hub.execute, and the chain the event already ran moves to then_0.
UEdGraphNode* InsertEventHub(UEdGraphNode& Event, UEdGraphPin& Then)
{
    FGraphNodeCreator<UK2Node_ExecutionSequence> Creator(*Event.GetGraph());
    UK2Node_ExecutionSequence* Hub = Creator.CreateNode(false);
    Hub->NodePosX = Event.NodePosX + 320;
    Hub->NodePosY = Event.NodePosY;
    Creator.Finalize();
    Hub->NodeComment = HubTag;
    const TArray<UEdGraphPin*> Chain = Then.LinkedTo;
    Then.BreakAllPinLinks();
    Then.MakeLinkTo(Hub->FindPin(UEdGraphSchema_K2::PN_Execute));
    for (UEdGraphPin* Old : Chain)
    {
        Hub->FindPin(TEXT("then_0"))->MakeLinkTo(Old);
    }
    return Hub;
}

UEdGraphPin* FreeHubPin(UEdGraphNode& Hub, const TSet<UEdGraphPin*>& Claimed)
{
    for (UEdGraphPin* Pin : Hub.Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() == 0 && !Claimed.Contains(Pin))
        {
            return Pin;
        }
    }
    // What UK2Node_ExecutionSequence::AddInputPin does: the first then_N not taken.
    int32 Index = 0;
    while (Hub.FindPin(FString::Printf(TEXT("then_%d"), Index)))
    {
        ++Index;
    }
    Hub.Modify();
    return Hub.CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FName(*FString::Printf(TEXT("then_%d"), Index)));
}
} // namespace

bool CheckHooks(UBlueprint* Blueprint, FPlan& Plan, FString& OutError, FString& OutCode)
{
    TSet<UEdGraph*> Pages;
    TSet<FString> SharedIds;
    for (int32 Index = 0; Index < Plan.Hooks.Num(); ++Index)
    {
        const FString Where = FString::Printf(TEXT("hooks[%d]"), Index);
        const FString Id = GetJsonStringField(Plan.Hooks[Index], TEXT("id"));
        FHookTarget Target;
        OutCode = TEXT("INVALID_RECIPE");
        if (!CheckKeys(Plan.Hooks[Index], {TEXT("id"), TEXT("event"), TEXT("customEvent"), TEXT("component")}, Where,
                       OutError))
        {
            return false;
        }
        // A custom event of this recipe is hookable when shared (it is made before the hooks
        // are wired); an owned one is wired from its own "$id.then".
        const FString Custom = GetJsonStringField(Plan.Hooks[Index], TEXT("customEvent"));
        const TSharedPtr<FJsonObject>* Declared = Custom.IsEmpty() ? nullptr : Plan.CustomEvents.FindByPredicate(
            [&Custom](const TSharedPtr<FJsonObject>& Event) { return GetJsonStringField(Event, TEXT("eventName")) == Custom; });
        if (Declared && !GetJsonBoolField(*Declared, TEXT("shared")))
        {
            OutError = FString::Printf(TEXT("%s hooks %s, a custom event this recipe owns; wire from its \"$id.then\"."),
                                       *Where, *Custom);
            return false;
        }
        const bool bNewShared = Declared && !FBlueprintEditorUtils::FindCustomEventNode(Blueprint, FName(*Custom));
        if (Id.IsEmpty() || (!bNewShared && !ResolveHookTarget(Blueprint, Plan.Hooks[Index], Target, OutError, OutCode)))
        {
            OutError = Id.IsEmpty() ? Where + TEXT(" needs an id: \"$id.then\" is where its logic starts.")
                                    : Where + TEXT(": ") + OutError;
            return false;
        }
        Pages.Add(Target.Existing ? Target.Existing->GetGraph() : nullptr);
        SharedIds.Add(Id + TEXT("_event"));
    }
    for (const TSharedPtr<FJsonObject>& Event : Plan.CustomEvents)
    {
        if (GetJsonBoolField(Event, TEXT("shared")))
        {
            SharedIds.Add(GetJsonStringField(Event, TEXT("id")));
        }
    }
    Pages.Remove(nullptr);
    OutCode = TEXT("HOOK_EVENTS_ON_DIFFERENT_PAGES");
    OutError = TEXT("The hooked events sit on different event graph pages; one recipe hooks events of one page. Move "
                    "them onto one page, or split the recipe.");
    if (Pages.Num() > 1)
    {
        return false;
    }
    Plan.Page = Pages.Num() == 1 ? *Pages.CreateConstIterator()
                                 : (Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0].Get() : nullptr);
    OutCode = TEXT("NO_EVENT_GRAPH");
    OutError = TEXT("This Blueprint has no event graph page for the recipe's steps.");
    if (!Plan.Page && (Plan.Ops.Num() > 0 || Plan.Hooks.Num() > 0 || Plan.CustomEvents.Num() > 0))
    {
        return false;
    }
    // An event's own exec pin belongs to its hub: wiring from it would unhook everyone else.
    // Both spellings: from "$x.then", and fromNodeId "$x" with fromPinName "then" (the
    // batch expands `from` over them, so `from` wins when a step has both).
    OutCode = TEXT("INVALID_RECIPE");
    for (int32 Index = 0; Index < Plan.Ops.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Step = Plan.Ops[Index]->AsObject();
        FString From = GetJsonStringField(Step, TEXT("from"));
        const FString FromNode = GetJsonStringField(Step, TEXT("fromNodeId"));
        From = !From.IsEmpty() || FromNode.IsEmpty() ? From
                                                     : FromNode + TEXT(".") + GetJsonStringField(Step, TEXT("fromPinName"));
        if (From.StartsWith(TEXT("$")) && From.EndsWith(TEXT(".then"), ESearchCase::IgnoreCase) &&
            SharedIds.Contains(From.Mid(1, From.Len() - 6)))
        {
            OutError = FString::Printf(TEXT("%s wires from %s, the exec pin of a shared event. Hook the event and wire "
                                            "from the hook's \"$id.then\" instead."), *Plan.Labels[Index], *From);
            return false;
        }
    }
    return true;
}

bool WireHooks(UBlueprint* Blueprint, const FPlan& Plan, FWiring& Out, FString& OutError, FString& OutCode)
{
    bool bNewEvents = false;
    for (const TSharedPtr<FJsonObject>& Event : Plan.CustomEvents)
    {
        const FName Name(*GetJsonStringField(Event, TEXT("eventName")));
        UEdGraphNode* Node = FBlueprintEditorUtils::FindCustomEventNode(Blueprint, Name);
        if (!Node)
        {
            FGraphNodeCreator<UK2Node_CustomEvent> Creator(*Plan.Page);
            UK2Node_CustomEvent* Created = Creator.CreateNode(false);
            Created->CustomFunctionName = Name;
            PlaceBelow(*Plan.Page, *Created);
            Creator.Finalize();
            Node = Created;
            bNewEvents = true;
            if (GetJsonBoolField(Event, TEXT("shared")))
            {
                Out.SharedTags.Add(Node->NodeGuid, HubTag);
            }
        }
        const FString Id = GetJsonStringField(Event, TEXT("id"));
        if (!Id.IsEmpty())
        {
            Out.NodeRefs.Add(Id, Node->NodeGuid.ToString());
        }
    }
    if (bNewEvents)
    {
        // Regenerates the skeleton class, where CallFunction steps find the new events.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    TSet<UEdGraphPin*> Claimed;
    for (const TSharedPtr<FJsonObject>& Hook : Plan.Hooks)
    {
        FHookTarget Target;
        if (!ResolveHookTarget(Blueprint, Hook, Target, OutError, OutCode))
        {
            return false;
        }
        UEdGraphNode* Event = Target.Existing ? Target.Existing : CreateHookEvent(*Plan.Page, Target, Out.SharedTags);
        UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
        if (!Then)
        {
            OutCode = TEXT("HOOK_EVENT_NOT_FOUND");
            OutError = FString::Printf(TEXT("event %s has no exec output to hook."), *Target.Label);
            return false;
        }
        UEdGraphNode* Hub = Then->LinkedTo.Num() == 1 ? Then->LinkedTo[0]->GetOwningNode() : nullptr;
        if (!IsEventHub(Hub))
        {
            Hub = InsertEventHub(*Event, *Then);
            Out.SharedTags.Add(Hub->NodeGuid, HubTag);
        }
        UEdGraphPin* Pin = FreeHubPin(*Hub, Claimed);
        Claimed.Add(Pin);
        const FString Id = GetJsonStringField(Hook, TEXT("id"));
        Out.NodeRefs.Add(Id, Hub->NodeGuid.ToString());
        Out.ThenPins.Add(Id, Pin->PinName.ToString());
        Out.NodeRefs.Add(Id + TEXT("_event"), Event->NodeGuid.ToString());
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), Id);
        Entry->SetStringField(TEXT("event"), Target.Label);
        Entry->SetStringField(TEXT("eventNodeGuid"), Event->NodeGuid.ToString());
        Entry->SetBoolField(TEXT("eventCreated"), Target.Existing == nullptr);
        Entry->SetBoolField(TEXT("parentCallAdded"), Target.Existing == nullptr && Target.ParentCall != nullptr);
        Entry->SetStringField(TEXT("hubNodeGuid"), Hub->NodeGuid.ToString());
        Entry->SetStringField(TEXT("pin"), Pin->PinName.ToString());
        Out.Report.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return true;
}

void RewriteRefs(const FPlan& Plan, const FWiring& Wiring)
{
    for (const TSharedPtr<FJsonValue>& Op : Plan.Ops)
    {
        const TSharedPtr<FJsonObject> Step = Op->AsObject();
        for (const TCHAR* Field : {TEXT("from"), TEXT("to")})
        {
            FString Ref;
            FString Node;
            FString Pin;
            if (!Step->TryGetStringField(Field, Ref) || !Ref.StartsWith(TEXT("$")) ||
                !Ref.RightChop(1).Split(TEXT("."), &Node, &Pin))
            {
                continue;
            }
            const FString* Guid = Wiring.NodeRefs.Find(Node);
            const FString* Then = Pin == TEXT("then") ? Wiring.ThenPins.Find(Node) : nullptr;
            if (Guid)
            {
                Step->SetStringField(Field, *Guid + TEXT(".") + (Then ? *Then : Pin));
            }
        }
        FString FromNode;
        FString FromPin;
        if (Step->TryGetStringField(TEXT("fromNodeId"), FromNode) && Step->TryGetStringField(TEXT("fromPinName"), FromPin) &&
            FromPin == TEXT("then") && Wiring.ThenPins.Contains(FromNode.RightChop(1)))
        {
            Step->SetStringField(TEXT("fromPinName"), Wiring.ThenPins[FromNode.RightChop(1)]);
        }
        for (const TCHAR* Field : {TEXT("fromNodeId"), TEXT("toNodeId"), TEXT("nodeId"), TEXT("nodeGuid")})
        {
            FString Ref;
            const FString* Guid = Step->TryGetStringField(Field, Ref) && Ref.StartsWith(TEXT("$"))
                ? Wiring.NodeRefs.Find(Ref.RightChop(1)) : nullptr;
            if (Guid)
            {
                Step->SetStringField(Field, *Guid);
            }
        }
    }
}
} // namespace McpBlueprintBehaviour::Detail
