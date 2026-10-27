#pragma once

#include "CoreMinimal.h"

// The graph node kinds behaviour recipes need, reached from the ordinary
// create_node and build_graph paths (so direct callers get them too). Declared
// here because McpBPGraphPrivate.h is full.
class FMulticastDelegateProperty;
class UBlueprint;
class UClass;

namespace McpBlueprintGraphHandlers
{
struct FActionContext;

/** create_node kinds: CallDelegate {memberName: dispatcher, memberClass for another class's},
 *  BindEvent {the same, plus eventName: a Bind Event node and a custom event with the dispatcher's
 *  inputs wired to it}, Message {memberClass: interface, memberName: its function}, AsyncTask
 *  {memberClass: task or async-action class, memberName: its static factory}. False for any other nodeType. */
bool TryCreateBehaviourNode(FActionContext& Context, const FString& NodeType, float X, float Y);

/** A class by path or name (a Blueprint's generated class too); null when none. */
UClass* ResolveBehaviourClass(const FString& Name);

/** A dispatcher on Owner, else this Blueprint's own (on the skeleton class, where a dispatcher
 *  added earlier in the same batch already is). */
FMulticastDelegateProperty* FindBehaviourDispatcher(UBlueprint* Blueprint, const FString& Name, UClass* Owner);

/** BindEvent, AddDelegate or K2Node_AddDelegate. */
bool IsBindEventKind(const FString& NodeType);

/** The BindEvent kind (McpBPBehaviourBindNodes.cpp); answers the call itself. */
bool CreateBindEventNode(FActionContext& Context, float X, float Y);

/** build_graph pre-check for those kinds: empty when the step resolves, else why, with
 *  OutCode set. Declared holds the names earlier steps of the batch declare. */
FString PrecheckBehaviourNode(const FActionContext& Context, const FString& NodeType, const FString& Member,
                              const FString& MemberClass, const TSet<FName>& Declared, FString& OutCode);
} // namespace McpBlueprintGraphHandlers
