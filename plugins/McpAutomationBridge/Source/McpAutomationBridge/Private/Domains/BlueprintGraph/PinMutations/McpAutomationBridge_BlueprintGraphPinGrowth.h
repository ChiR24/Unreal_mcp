#pragma once

#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

namespace McpBlueprintGraphHandlers
{
// The pin a link names, grown on demand the way the editor's "Add pin" does: a Sequence, Make Array or commutative
// math node (then_2, [2], C) or a Switch on Int (case 7). Pins added in vain are removed again.
UEdGraphPin* FindOrGrowPin(FActionContext& Context, UEdGraphNode* Node, const FString& PinName);
}
