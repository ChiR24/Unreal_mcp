#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

// The lint's rules (McpBPLintRules.cpp), each adding what it finds in one graph, or in the Blueprint's variables.
namespace McpBPLint
{
struct FFinding
{
    const TCHAR* Rule;
    const TCHAR* Severity;
    const UEdGraphNode* Node;
    FString Detail;
};

void TickHeavy(const UEdGraph* Graph, TArray<FFinding>& Out);
void Unreachable(const UEdGraph* Graph, TArray<FFinding>& Out);
void RandomFanout(const UEdGraph* Graph, TArray<FFinding>& Out);
void InfiniteWhile(const UEdGraph* Graph, TArray<FFinding>& Out);
void NeverAssigned(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FFinding>& Out);
/** McpBPLintGuards.cpp: uses of an object variable that can be None, on a path no validity check of it dominates. */
void UnguardedRead(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TArray<FFinding>& Out);
/** McpBPLintNumeric.cpp: divide_by_zero, index_out_of_range, out_of_range, float_equality, constant_condition and
 * endless_loop, those On allows, from the values an abstract interpreter finds the variables can hold. Returns how many
 * events or functions had more paths than the interpreter follows (nothing is said of their values). */
int32 NumericRules(const UBlueprint* Blueprint, const TArray<UEdGraph*>& Graphs, TFunctionRef<bool(const TCHAR*)> On, TArray<FFinding>& Out);
} // namespace McpBPLint
