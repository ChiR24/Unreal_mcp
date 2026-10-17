// McpBPActorBinding.h — graph nodes that are about one placed actor, shared by the level Blueprint door
// (manage_level_structure add_level_blueprint_node) and the Blueprint graph door (create_node).
#pragma once

#include "CoreMinimal.h"

class FMulticastDelegateProperty;
class UClass;
class UEdGraphNode;
class UK2Node;
class ULevel;

// The multicast delegate of Class named Name (OnActorBeginOverlap, or its OnActorBeginOverlap__ spelling), or null.
// OutAssignable gets the BlueprintAssignable ones, the only events a node can bind.
FMulticastDelegateProperty* McpFindMulticastDelegate(const UClass* Class, const FString& Name, TArray<FString>& OutAssignable);

// A K2Node_ActorBoundEvent or K2Node_Literal is about one placed actor of Level (null outside a level Blueprint),
// named by ActorName; a bound event also takes EventName, one of that actor's event dispatchers. Bound here, before
// the node's pins exist, since they come from the actor's class or the event's signature. False with OutError and
// OutCode when it cannot be; true and untouched for any other node given no ActorName.
bool McpBindLevelActorNode(UK2Node* Node, ULevel* Level, const FString& ActorName, const FString& EventName,
                           FString& OutError, FString& OutCode);

// A node's pins as "name (in|out)", the names a connect call takes; hidden ones (a world context) are never wired.
FString McpDescribeNodePins(const UEdGraphNode* Node);
