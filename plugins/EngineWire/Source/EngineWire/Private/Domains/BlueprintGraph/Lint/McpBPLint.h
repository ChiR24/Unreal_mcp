#pragma once

#include "CoreMinimal.h"

namespace McpBlueprintGraphHandlers
{
struct FActionContext;
}

// manage_blueprint lint_blueprint: the mistakes a read of a Blueprint's graphs finds before Play does, each with the
// node to fix. Rules: tick_heavy (Event Tick reaches a call that searches the level or spawns), unreachable (no event or
// function entry leads to a node), random_fanout (one Random node read by several nodes rolls once per read),
// infinite_while (a While Loop whose condition is always true), never_assigned (an object variable read but never set,
// not editable per instance nor exposed on spawn, so every read is None), unguarded_read (an object variable that can be
// None used as a target on a path with no validity check). From the values the variables can hold on each path:
// divide_by_zero, index_out_of_range, out_of_range (a Set outside the variable's Value Range), float_equality (== on a
// computed float), constant_condition (a Branch that always goes one way) and endless_loop (a While Loop whose body can
// never make its condition false). Reads only: nothing compiles or changes.
namespace McpBPLint
{
/** False for any other sub-action. */
bool HandleLint(const McpBlueprintGraphHandlers::FActionContext& Context);
} // namespace McpBPLint
