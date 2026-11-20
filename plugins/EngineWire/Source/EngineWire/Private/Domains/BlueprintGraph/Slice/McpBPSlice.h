#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UBlueprint;
class UEdGraphNode;

namespace McpBlueprintGraphHandlers
{
struct FActionContext;
}

// manage_blueprint slice_variable: every node in the project's Blueprints that sets or reads one member variable, each
// writer with the events and functions that reach it, the Branches it sits behind (and which exit), and the callers of
// the function or custom event it runs in. A variable set and read nowhere is a dead write. Reads only.
namespace McpBPSlice
{
/** False for any other sub-action. */
bool HandleSlice(const McpBlueprintGraphHandlers::FActionContext& Context);

/**
 * Whether Node gets or sets Variable declared on Owner (or inherited by it): from Owner's own graphs, a child
 * Blueprint's, or another Blueprint's get or set on an object of Owner's class. bOutSet says which.
 */
bool Touches(const UEdGraphNode* Node, const UBlueprint* Owner, FName Variable, bool& bOutSet);

/** The slice over the Blueprints in Scope: writers (with entries, conditions, callers), readers, their counts, deadWrite. */
TSharedPtr<FJsonObject> Slice(const UBlueprint* Owner, FName Variable, const TArray<UBlueprint*>& Scope);
} // namespace McpBPSlice
