// What recipe references name: the events hooks attach to (an overridable event, a
// component delegate, an existing custom event) and the step ids "$id" resolves.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintBehaviour::Detail
{
namespace
{
UFunction* FindOverridableEvent(UBlueprint* Blueprint, FString Name)
{
    Name.RemoveFromStart(TEXT("Event "));
    // The editor shows AActor's Receive<Name> events without the prefix.
    for (const FString& Candidate : {Name, FString(TEXT("Receive")) + Name})
    {
        for (UClass* Class = Blueprint->ParentClass; Class; Class = Class->GetSuperClass())
        {
            UFunction* Function = Class->FindFunctionByName(FName(*Candidate), EIncludeSuperFlag::ExcludeSuper);
            if (Function && Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) && !Function->GetReturnProperty())
            {
                return Function;
            }
        }
    }
    return nullptr;
}

bool ResolveComponentEvent(UBlueprint* Blueprint, const FString& Component, const FString& Event, FHookTarget& Out,
                           FString& OutError)
{
    Out.Label = Component + TEXT(".") + Event;
    UClass* Generated = Blueprint->GeneratedClass;
    for (TFieldIterator<FObjectProperty> It(Generated); Generated && It && !Out.Component; ++It)
    {
        if (It->GetName().Equals(Component, ESearchCase::IgnoreCase) && It->PropertyClass &&
            It->PropertyClass->IsChildOf(UActorComponent::StaticClass()))
        {
            Out.Component = *It;
        }
    }
    UClass* ComponentClass = Out.Component ? static_cast<UClass*>(Out.Component->PropertyClass) : nullptr;
    for (TFieldIterator<FMulticastDelegateProperty> It(ComponentClass); ComponentClass && It && !Out.Delegate; ++It)
    {
        if (It->GetName().Equals(Event, ESearchCase::IgnoreCase) && It->HasAnyPropertyFlags(CPF_BlueprintAssignable))
        {
            Out.Delegate = *It;
        }
    }
    OutError = FString::Printf(TEXT("no component '%s' with a Blueprint-assignable delegate '%s' on this Blueprint "
                                    "(add the component before the call)."), *Component, *Event);
    TArray<UK2Node_ComponentBoundEvent*> Bound;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Bound);
    for (UK2Node_ComponentBoundEvent* Node : Bound)
    {
        if (Out.Delegate && Node->ComponentPropertyName == Out.Component->GetFName() &&
            Node->DelegatePropertyName == Out.Delegate->GetFName())
        {
            Out.Existing = Node;
        }
    }
    return Out.Delegate != nullptr;
}
} // namespace

bool ResolveHookTarget(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Hook, FHookTarget& Out, FString& OutError,
                       FString& OutCode)
{
    const FString Event = GetJsonStringField(Hook, TEXT("event"));
    const FString Custom = GetJsonStringField(Hook, TEXT("customEvent"));
    const FString Component = GetJsonStringField(Hook, TEXT("component"));
    OutCode = TEXT("HOOK_EVENT_NOT_FOUND");
    if (!Custom.IsEmpty())
    {
        Out.Label = Custom;
        Out.Existing = FBlueprintEditorUtils::FindCustomEventNode(Blueprint, FName(*Custom));
        OutError = FString::Printf(TEXT("custom event '%s' does not exist. The behaviour that provides it declares it in "
                                        "customEvents with shared true; run that one first."), *Custom);
        if (Out.Existing && IsBehaviourOwned(*Out.Existing))
        {
            OutCode = TEXT("HOOK_EVENT_OWNED");
            OutError = FString::Printf(TEXT("custom event '%s' belongs to another behaviour and is rebuilt whenever that "
                                            "one re-runs; it must declare it with shared true to be hookable."), *Custom);
            return false;
        }
        return Out.Existing != nullptr;
    }
    if (Event.IsEmpty())
    {
        OutError = TEXT("a hook names event, customEvent, or component plus event.");
        return false;
    }
    if (!Component.IsEmpty())
    {
        return ResolveComponentEvent(Blueprint, Component, Event, Out, OutError);
    }
    Out.Label = Event;
    Out.Function = FindOverridableEvent(Blueprint, Event);
    const UClass* Parent = Blueprint->ParentClass;
    // A new override hides what implements the event above this Blueprint (a parent
    // Blueprint's graph, a BlueprintNativeEvent's C++), so it calls that first, as the
    // editor's default events do.
    UFunction* Above = Out.Function && Parent ? Parent->FindFunctionByName(Out.Function->GetFName()) : nullptr;
    Out.ParentCall =
        Above && (Above->HasAnyFunctionFlags(FUNC_Native) || Cast<UBlueprintGeneratedClass>(Above->GetOuter())) ? Above
                                                                                                                  : nullptr;
    OutError = FString::Printf(TEXT("'%s' is not an event %s or its parents let a Blueprint implement."), *Event,
                               Parent ? *Parent->GetName() : TEXT("this Blueprint's parent class"));
    TArray<UK2Node_Event*> Events;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Events);
    for (UK2Node_Event* Node : Events)
    {
        if (Out.Function && Node->bOverrideFunction && Node->EventReference.GetMemberName() == Out.Function->GetFName())
        {
            Out.Existing = Node;
        }
    }
    return Out.Function != nullptr;
}

void PlaceBelow(const UEdGraph& Page, UEdGraphNode& Node)
{
    int32 Bottom = 0;
    for (const UEdGraphNode* Other : Page.Nodes)
    {
        if (Other && Other != &Node)
        {
            Bottom = FMath::Max(Bottom, Other->NodePosY + 400);
        }
    }
    Node.NodePosX = 0;
    Node.NodePosY = Bottom;
}

UEdGraphNode* CreateHookEvent(UEdGraph& Page, const FHookTarget& Target, TMap<FGuid, FString>& SharedTags)
{
    UEdGraphNode* Event = nullptr;
    if (Target.Delegate)
    {
        FGraphNodeCreator<UK2Node_ComponentBoundEvent> Creator(Page);
        UK2Node_ComponentBoundEvent* Node = Creator.CreateNode(false);
        Node->InitializeComponentBoundEventParams(Target.Component, Target.Delegate);
        PlaceBelow(Page, *Node);
        Creator.Finalize();
        Event = Node;
    }
    else
    {
        FGraphNodeCreator<UK2Node_Event> Creator(Page);
        UK2Node_Event* Node = Creator.CreateNode(false);
        Node->EventReference.SetFromField<UFunction>(Target.Function, false);
        Node->bOverrideFunction = true;
        PlaceBelow(Page, *Node);
        Creator.Finalize();
        Event = Node;
    }
    SharedTags.Add(Event->NodeGuid, HubTag); // another behaviour may hook it too
    if (Target.ParentCall)
    {
        // What FKismetEditorUtilities::AddDefaultEventNode does: every event pin feeds the
        // parent call, and the event runs it first (the hub keeps it on then_0).
        FGraphNodeCreator<UK2Node_CallParentFunction> ParentCreator(Page);
        UK2Node_CallParentFunction* Parent = ParentCreator.CreateNode(false);
        Parent->SetFromFunction(Target.ParentCall);
        Parent->NodePosX = Event->NodePosX + 320;
        Parent->NodePosY = Event->NodePosY;
        ParentCreator.Finalize();
        for (UEdGraphPin* Pin : Event->Pins)
        {
            if (UEdGraphPin* Input = Pin ? Parent->FindPin(Pin->PinName, EGPD_Input) : nullptr)
            {
                Input->MakeLinkTo(Pin);
            }
        }
        UEdGraphPin* Exec = Parent->FindPin(UEdGraphSchema_K2::PN_Execute);
        UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
        if (Exec && Then)
        {
            Exec->MakeLinkTo(Then);
        }
        SharedTags.Add(Parent->NodeGuid, HubTag);
    }
    return Event;
}

// Ids name nodes for "$id" references and must be unique over the whole batch.
bool CheckRecipeIds(const FPlan& Plan, FString& OutError)
{
    TSet<FString> Seen;
    auto Claim = [&Seen, &OutError](const FString& Id, const FString& Where)
    {
        bool bDuplicate = false;
        Seen.Add(Id, &bDuplicate);
        if (bDuplicate)
        {
            OutError = FString::Printf(TEXT("%s reuses id '%s'; every id in a recipe must be unique."), *Where, *Id);
        }
        return !bDuplicate;
    };
    for (int32 Index = 0; Index < Plan.Ops.Num(); ++Index)
    {
        const FString Id = GetJsonStringField(Plan.Ops[Index]->AsObject(), TEXT("id"));
        if (!Id.IsEmpty() && (!Claim(Id, Plan.Labels[Index]) || !Claim(Id + TEXT("_return"), Plan.Labels[Index])))
        {
            return false;
        }
    }
    for (const TSharedPtr<FJsonObject>& Hook : Plan.Hooks)
    {
        const FString Id = GetJsonStringField(Hook, TEXT("id"));
        if (!Id.IsEmpty() && (!Claim(Id, TEXT("hooks")) || !Claim(Id + TEXT("_event"), TEXT("hooks"))))
        {
            return false;
        }
    }
    for (const TSharedPtr<FJsonObject>& Event : Plan.CustomEvents)
    {
        const FString Id = GetJsonStringField(Event, TEXT("id"));
        if (!Id.IsEmpty() && !Claim(Id, TEXT("customEvents")))
        {
            return false;
        }
    }
    return true;
}
} // namespace McpBlueprintBehaviour::Detail
