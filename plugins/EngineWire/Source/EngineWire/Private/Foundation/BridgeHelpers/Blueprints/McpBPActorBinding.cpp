// Issue #675: a K2Node_ActorBoundEvent made by class name had no actor and no delegate ("None (None)"), which broke
// the level Blueprint's compile, and a K2Node_Literal had no object ("Unknown"); both reported success.
#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/Actor.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_Literal.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

namespace
{
// The actor of Level that Name names. Only Level is searched: a level Blueprint references its own level's actors, and
// a whole-world search found a same-labeled actor of another level first. A name or path is unique; a label is not,
// so two actors carrying it are named back instead of binding whichever came first.
AActor* FindLevelActorForBinding(ULevel& Level, const FString& Name, FString& OutError, FString& OutCode)
{
    TArray<AActor*> Labeled;
    for (AActor* Actor : Level.Actors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }
        if (Actor->GetName().Equals(Name, ESearchCase::IgnoreCase) ||
            (Name.StartsWith(TEXT("/")) && Actor->GetPathName().Equals(Name, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
        if (Actor->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
        {
            Labeled.Add(Actor);
        }
    }
    if (Labeled.Num() == 1)
    {
        return Labeled[0];
    }
    const FString LevelName = Level.GetOutermost()->GetName();
    if (Labeled.Num() > 1)
    {
        TArray<FString> Names;
        for (const AActor* Actor : Labeled)
        {
            Names.Add(Actor->GetName());
        }
        OutCode = TEXT("ACTOR_AMBIGUOUS");
        OutError = FString::Printf(TEXT("%d actors in %s are labeled '%s': pass one by name (%s)."), Labeled.Num(),
                                   *LevelName, *Name, *FString::Join(Names, TEXT(", ")));
        return nullptr;
    }
    // An actor of another loaded level gets its own code: ACTOR_NOT_FOUND brings both gateways' "No actor in the world
    // is named ..." hint, which is wrong for an actor that is there.
    const AActor* Elsewhere = FindActorByNameInWorldForMcp(Level.OwningWorld, Name, true);
    OutCode = Elsewhere && Elsewhere->GetLevel() ? TEXT("ACTOR_IN_OTHER_LEVEL") : TEXT("ACTOR_NOT_FOUND");
    OutError = Elsewhere && Elsewhere->GetLevel()
        ? FString::Printf(TEXT("Actor '%s' is in level %s, not %s: a level Blueprint references only its own level's "
                               "actors, so edit %s's (levelPath)."),
                          *Name, *Elsewhere->GetLevel()->GetOutermost()->GetName(), *LevelName,
                          *Elsewhere->GetLevel()->GetOutermost()->GetName())
        : FString::Printf(TEXT("Actor '%s' not found in %s."), *Name, *LevelName);
    return nullptr;
}
}

FMulticastDelegateProperty* McpFindMulticastDelegate(const UClass* Class, const FString& Name, TArray<FString>& OutAssignable)
{
    FMulticastDelegateProperty* Found = nullptr;
    for (TFieldIterator<FMulticastDelegateProperty> It(Class); It; ++It)
    {
        const FString PropName = It->GetName();
        if (It->HasAnyPropertyFlags(CPF_BlueprintAssignable))
        {
            OutAssignable.Add(PropName);
        }
        if (!Found && !Name.IsEmpty() &&
            (PropName.Equals(Name, ESearchCase::IgnoreCase) || PropName.StartsWith(Name + TEXT("__"), ESearchCase::IgnoreCase)))
        {
            Found = *It;
        }
    }
    return Found;
}

bool McpBindLevelActorNode(UK2Node* Node, ULevel* Level, const FString& ActorName, const FString& EventName,
                           FString& OutError, FString& OutCode)
{
    UK2Node_ActorBoundEvent* BoundEvent = Cast<UK2Node_ActorBoundEvent>(Node);
    UK2Node_Literal* Literal = Cast<UK2Node_Literal>(Node);
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
    const FString NodeClass = Node->GetClass()->GetName();
    if (!Level)
    {
        OutError = FString::Printf(TEXT("%s belongs in a level Blueprint, which references its level's placed actors: "
                                        "add it with manage_level_structure edit_level_blueprint (edit add_node)."), *NodeClass);
        return false;
    }
    if (ActorName.IsEmpty())
    {
        OutError = FString::Printf(TEXT("%s needs actorName: the placed actor it is about (label, name or object path)."), *NodeClass);
        return false;
    }
    AActor* Actor = FindLevelActorForBinding(*Level, ActorName, OutError, OutCode);
    if (!Actor)
    {
        return false;
    }
    if (Literal)
    {
        // AllocateDefaultPins types the output pin from ObjectRef, so set it before the pins are made.
        Literal->SetObjectRef(Actor);
        return true;
    }
    // Only BlueprintAssignable dispatchers can drive an event, as in the editor's "Add Event" list for the actor.
    TArray<FString> Assignable;
    FMulticastDelegateProperty* Delegate = McpFindMulticastDelegate(Actor->GetClass(), EventName, Assignable);
    if (!Delegate || !Delegate->HasAnyPropertyFlags(CPF_BlueprintAssignable))
    {
        const FString Events = FString::Join(Assignable, TEXT(", "));
        OutCode = EventName.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("DELEGATE_NOT_FOUND");
        OutError = EventName.IsEmpty()
            ? FString::Printf(TEXT("K2Node_ActorBoundEvent needs eventName, one of %s's events: %s."), *Actor->GetActorLabel(), *Events)
            : FString::Printf(TEXT("'%s' is not an event %s can bind; its events: %s."), *EventName, *Actor->GetActorLabel(), *Events);
        return false;
    }
    // One node per actor and event, as the editor keeps it; a second would run the same logic twice.
    if (const UK2Node_ActorBoundEvent* Existing = FKismetEditorUtilities::FindBoundEventForActor(Actor, Delegate->GetFName()))
    {
        OutCode = TEXT("ALREADY_EXISTS");
        OutError = FString::Printf(TEXT("The level Blueprint already has %s for %s: node %s (%s). Connect to it instead."),
                                   *Delegate->GetName(), *Actor->GetActorLabel(), *Existing->GetName(),
                                   *Existing->NodeGuid.ToString());
        return false;
    }
    BoundEvent->InitializeActorBoundEventParams(Actor, Delegate);
    return true;
}

FString McpDescribeNodePins(const UEdGraphNode* Node)
{
    if (!Node)
    {
        return TEXT("<node not found>");
    }
    TArray<FString> Names;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && !Pin->bHidden)
        {
            Names.Add(FString::Printf(TEXT("%s (%s)"), *Pin->GetName(), Pin->Direction == EGPD_Output ? TEXT("out") : TEXT("in")));
        }
    }
    return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("<none>");
}
