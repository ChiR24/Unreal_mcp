#pragma once

#include "CoreMinimal.h"
#include "Domains/BlueprintGraph/Lint/McpBPLintFlow.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

// The rows a slice lists (McpBPSlice.cpp), built in McpBPSliceWriters.cpp.
namespace McpBPSliceDetail
{
/** Most rows in one list (writers, readers, a function's callers). */
inline constexpr int32 MaxRows = 100;

/** blueprint (package path), graph, node (title) and nodeId (as edit_graph takes it). */
TSharedPtr<FJsonObject> NodeRow(const UBlueprint* Blueprint, const UEdGraphNode* Node);

/** Whether Owner is on Class's chain: Class is Owner's own class (skeleton or generated) or one derived from it. */
bool SameBlueprintChain(const UClass* Class, const UBlueprint* Owner);

/** A Set's row: NodeRow plus the entries that reach it (with their callers in Scope) and the Branches it sits behind. */
TSharedPtr<FJsonObject> WriterRow(const UBlueprint* Blueprint, const UEdGraphNode* Node, const TArray<UBlueprint*>& Scope,
                                  TMap<const UEdGraph*, McpBPLint::FFlow>& Flows);
} // namespace McpBPSliceDetail
