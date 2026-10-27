#pragma once

#include "CoreMinimal.h"

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
} // namespace McpBPLint
