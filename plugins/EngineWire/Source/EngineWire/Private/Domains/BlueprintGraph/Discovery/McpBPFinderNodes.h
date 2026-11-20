#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace McpBlueprintGraphHandlers
{
// The create_node node types a find_functions request names that are nodes, not functions (Spawn Actor from Class, a
// loop, a branch, a cast): each {nodeType, how}, and in OutNote one sentence saying so; empty when it names none.
// Recorded runs asked the finder for "spawn actor from class", "for each loop" and "branch if condition" and got
// unrelated functions back.
TArray<TSharedPtr<FJsonValue>> FinderNodeTypes(const TArray<FString>& Wanted, FString& OutNote);
}
