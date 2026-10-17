// McpLevelStructureBPActorBinding.cpp — the placed actor an add_level_blueprint_node node is about.
//
// Issue #675: a K2Node_ActorBoundEvent made by class name had no actor and no delegate ("None (None)"), which broke
// the level blueprint's compile, and a K2Node_Literal had no object ("Unknown"). Both reported success. actorName
// names the placed actor (label, name or object path); eventName the event dispatcher a bound event fires on.
#include "Domains/LevelStructure/McpLevelStructureActions.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_Literal.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace McpLevelStructure
{

bool BindLevelBlueprintNodeToActor(UK2Node* Node, ULevel* Level, const TSharedPtr<FJsonObject>& Payload, FString& OutError, FString& OutCode)
{
    UK2Node_ActorBoundEvent* BoundEvent = Cast<UK2Node_ActorBoundEvent>(Node);
    UK2Node_Literal* Literal = Cast<UK2Node_Literal>(Node);
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    OutCode = TEXT("INVALID_ARGUMENT");
    if (!BoundEvent && !Literal)
    {
        if (ActorName.IsEmpty())
        {
            return true;
        }
        OutError = TEXT("actorName applies to an actor reference (K2Node_Literal) or an actor-bound event (K2Node_ActorBoundEvent) only.");
        return false;
    }
    if (ActorName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("%s needs actorName: the placed actor it is about (label, name or object path)."),
                                   *Node->GetClass()->GetName());
        return false;
    }
    // A level blueprint references only its own level's actors; a sublevel's actor needs that sublevel's levelPath.
    AActor* Actor = FindActorByNameInWorldForMcp(Level->OwningWorld, ActorName, true);
    if (!Actor || Actor->GetLevel() != Level)
    {
        OutCode = TEXT("ACTOR_NOT_FOUND");
        OutError = Actor
            ? FString::Printf(TEXT("Actor '%s' is in level %s, not %s: pass that level as levelPath."), *ActorName,
                              *Actor->GetLevel()->GetOutermost()->GetName(), *Level->GetOutermost()->GetName())
            : FString::Printf(TEXT("Actor '%s' not found in %s."), *ActorName, *Level->GetOutermost()->GetName());
        return false;
    }
    if (Literal)
    {
        // AllocateDefaultPins types the output pin from ObjectRef, so set it before the pins are made.
        Literal->SetObjectRef(Actor);
        return true;
    }
    // Only BlueprintAssignable dispatchers can drive an event, as in the editor's "Add Event" list for the actor.
    const FString EventName = GetJsonStringField(Payload, TEXT("eventName"), TEXT(""));
    FMulticastDelegateProperty* Delegate = nullptr;
    TArray<FString> Assignable;
    for (TFieldIterator<FMulticastDelegateProperty> It(Actor->GetClass()); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_BlueprintAssignable))
        {
            Assignable.Add(It->GetName());
            if (!Delegate && It->GetName().Equals(EventName, ESearchCase::IgnoreCase))
            {
                Delegate = *It;
            }
        }
    }
    if (!Delegate)
    {
        OutCode = EventName.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("DELEGATE_NOT_FOUND");
        OutError = FString::Printf(TEXT("K2Node_ActorBoundEvent needs eventName, one of %s's events: %s."),
                                   *Actor->GetActorLabel(), *FString::Join(Assignable, TEXT(", ")));
        return false;
    }
    // One node per actor and event, as the editor keeps it; a second would run the same logic twice.
    if (const UK2Node_ActorBoundEvent* Existing = FKismetEditorUtilities::FindBoundEventForActor(Actor, Delegate->GetFName()))
    {
        OutCode = TEXT("ALREADY_EXISTS");
        OutError = FString::Printf(TEXT("The level blueprint already has %s for %s: node %s. Connect to it instead."),
                                   *Delegate->GetName(), *Actor->GetActorLabel(), *Existing->GetName());
        return false;
    }
    BoundEvent->InitializeActorBoundEventParams(Actor, Delegate);
    return true;
}

}
