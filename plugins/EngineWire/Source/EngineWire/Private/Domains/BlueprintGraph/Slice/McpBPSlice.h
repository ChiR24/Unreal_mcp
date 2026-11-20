#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
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

/** manage_blueprint slice_function (McpBPSliceCalls.cpp): every call of one function or custom event in Scope, each with
 *  the variable slice's row (entries, Branches, callers); uncalled when none. */
bool HandleSliceFunction(const McpBlueprintGraphHandlers::FActionContext& Context);

/** The calls slice_function finds: a dispatcher's Calls (its broadcasts) as callers and its Binds and Assigns as
 *  binders, else every call of the function or custom event; Error names Owner's own when it has no such member. */
struct FCallSlice
{
    TArray<TSharedPtr<FJsonValue>> Callers, Binders;
    int32 Count = 0, BinderCount = 0;
    bool bDispatcher = false;
    FString Error;
};
FCallSlice SliceCalls(UBlueprint* Owner, FName Function, const TArray<UBlueprint*>& Scope);

/** manage_blueprint explain_variable and explain_function (McpBPExplain.cpp): arm marks the slice's writers (or a
 *  function's calls) and the Branches before them to record the next Play; read says, row by row, whether it ran, else
 *  the first Branch that did not let it. */
bool HandleExplain(const McpBlueprintGraphHandlers::FActionContext& Context);
} // namespace McpBPSlice
