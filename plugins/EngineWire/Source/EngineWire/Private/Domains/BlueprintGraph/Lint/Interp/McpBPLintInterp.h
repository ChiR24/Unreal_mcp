#pragma once

#include "CoreMinimal.h"
#include "Domains/BlueprintGraph/Lint/Interp/McpBPLintEval.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

// The lint's abstract interpreter: each entry of a graph (an event, a function) run to a fixpoint over its exec flow,
// giving the state every exec node starts in (McpBPLintInterp.cpp, McpBPLintStep.cpp), from what the Blueprint's
// variables hold where an entry starts (McpBPLintEntry.cpp) and what each node does to them (McpBPLintApply.cpp).
namespace McpBPLint
{
/** The member variables the interpreter follows, as every entry finds them, and their Value Ranges. */
struct FMembers
{
    FState Entry;
    TSet<FName> Names;
    TMap<FName, FRange> Ranges;

    /** After a call that may set members (anything but the engine's math, string, array and system libraries). */
    void Forget(FState& State) const;
};

/** Numeric and Boolean members by their Value Range, or, where only this Blueprint can set them (private, not editable
 * on an instance nor exposed on spawn), by their default and every value its Set nodes store; the rest unknown. Arrays
 * of that kind no node writes, by their default length. */
FMembers ReadMembers(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs);

/** A function's local variables at its start, by their defaults, and the inputs a function's or custom event's callers
 * pass, unknown (a value the graph should check before it divides by it or indexes with it). */
void AddLocals(const UEdGraph* Graph, FState& State);

/** What running Node does to the variables State follows. */
void Apply(const UEdGraphNode* Node, FState& State, const FMembers& Members);

/** An exec node and a state it starts in; a node can run in several (after a Sequence's earlier outputs, in a loop). */
struct FRun
{
    const UEdGraphNode* Node = nullptr;
    FState State;
};

struct FAnalysis
{
    TArray<FRun> Runs;
    /** A loop's body comes back to it in these states. */
    TMap<const UEdGraphNode*, TArray<FState>> LoopReturns;
    /** Entries given up on: more paths than the budget, so nothing is said of them. */
    int32 Skipped = 0;
};

FAnalysis Interpret(const UEdGraph* Graph, const FMembers& Members);
} // namespace McpBPLint
