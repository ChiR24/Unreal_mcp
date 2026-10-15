#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;
class UEdGraphNode;

// Graphs the compiler accepts that do not do what they say. Each check adds one warning diagnostic per finding while
// Diagnostics holds fewer than MaxDiagnostics.

// One warning diagnostic, {severity, message}, for a finding.
void McpAddCheckWarning(TArray<TSharedPtr<FJsonValue>>& Diagnostics, const FString& Message);

// A node as a finding names it: 'Title' (NodeName).
FString McpNodeLabel(const UEdGraphNode* Node);

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

// A loop runs all its passes at once. A latent node (Delay, Move Component To) in its body holds one wait per
// Blueprint, so the later passes start no wait of their own and what follows the node runs once, after the loop: a
// wave of enemies spawned one second apart spawned one enemy, a second after the loop.
void McpAddLoopLatentWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);

// A timer set again, or a Retriggerable Delay called again, starts its wait over. Reached from Event Tick with no
// branch, switch or gate between, it runs every frame, so a wait longer than a frame never ends: a one-second timer
// set from Tick never fired.
void McpAddTickRestartWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
