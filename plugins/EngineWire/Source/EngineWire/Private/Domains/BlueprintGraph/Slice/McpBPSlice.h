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

/** Owner, then the Blueprints under Folder, at most Cap in all; OutSkipped counts those past it. */
TArray<UBlueprint*> ScopeFor(UBlueprint* Owner, const FString& Folder, int32 Cap, int32& OutSkipped);

/** manage_blueprint explain_variable (McpBPExplain.cpp): arm marks the slice's writers and the Branches before them to
 *  record the next Play; read says, writer by writer, whether it ran, else the first Branch that did not let it. */
bool HandleExplain(const McpBlueprintGraphHandlers::FActionContext& Context);
} // namespace McpBPSlice
