#pragma once

#include "CoreMinimal.h"

// The graph node kinds behaviour recipes need, reached from the ordinary
// create_node and build_graph paths (so direct callers get them too). Declared
// here because McpAutomationBridge_BlueprintGraphHandlersPrivate.h is full.
namespace McpBlueprintGraphHandlers
{
struct FActionContext;

/** create_node kinds: CallDelegate {memberName: dispatcher, memberClass for another class's},
 *  Message {memberClass: interface, memberName: its function}, AsyncTask {memberClass: task or
 *  async-action class, memberName: its static factory}. False for any other nodeType. */
bool TryCreateBehaviourNode(FActionContext& Context, const FString& NodeType, float X, float Y);

/** build_graph pre-check for those kinds: empty when the step resolves, else why, with
 *  OutCode set. Declared holds the names earlier steps of the batch declare. */
FString PrecheckBehaviourNode(const FActionContext& Context, const FString& NodeType, const FString& Member,
                              const FString& MemberClass, const TSet<FName>& Declared, FString& OutCode);
} // namespace McpBlueprintGraphHandlers
