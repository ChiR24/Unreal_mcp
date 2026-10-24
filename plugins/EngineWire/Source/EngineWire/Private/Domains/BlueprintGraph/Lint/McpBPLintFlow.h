#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

// One graph's exec flow and its dominators (McpBPLintFlow.cpp), for the lint rules that ask what every path to a node
// passes through first.
namespace McpBPLint
{
/**
 * Vertex 0 is a root before every entry, then one vertex per node with an exec pin, and one more per validity check for
 * its valid exit (the Is Valid macro, a Branch on Is Valid, a validated get, an impure cast), so a use past that exit is
 * dominated by it and a use past the check's other exits is not.
 */
struct FFlow
{
    TMap<const UEdGraphNode*, int32> Vertex;
    TArray<FName> Vouches; // per vertex: the variable a valid exit vouches for, else NAME_None
    TArray<TArray<int32>> Next;
    TArray<int32> Idom; // INDEX_NONE: no entry reaches it
};

bool IsExecPin(const UEdGraphPin* Pin);
/** Whether Node takes part in the exec flow: it has an exec pin, so it is not a pure node. */
bool HasExecPin(const UEdGraphNode* Node);
/** The output an input's value comes from, past any reroute knots. */
const UEdGraphPin* SourceOf(const UEdGraphPin* Input);
/** Graph's exec flow with its immediate dominators. */
FFlow BuildFlow(const UEdGraph* Graph);
/** Whether every path to the vertex At passes a valid exit that vouches for Variable. */
bool Guarded(const FFlow& Flow, int32 At, FName Variable);
} // namespace McpBPLint
