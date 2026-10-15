#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

// Graphs the compiler accepts that do not do what they say. Each check adds one warning diagnostic per finding while
// Diagnostics holds fewer than MaxDiagnostics.

// A pure node runs again for each node that reads it. One that reads a variable and feeds that variable's Set node
// also gives each node that runs after the Set the new value worked on again: Laps + 1 wired into Set Laps and into a
// Print after it printed Laps + 2, and a check against the goal passed a lap early. The fix is the Set node's own
// output pin.
void McpAddStaleReadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);

// The same rerun gives every node that reads a random value its own: one Random Integer feeding the Get that showed a
// card and the Remove that took one out of the deck showed one card and removed another. The fix is to store the value
// with a Set node first and read that.
void McpAddRandomReadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);

// A timer by function name (Set Timer by Function Name, Clear Timer ...) on the Blueprint itself names its function or
// event as text: a name the Blueprint has no function or event for compiles, and the timer never runs.
void McpAddTimerNameWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
