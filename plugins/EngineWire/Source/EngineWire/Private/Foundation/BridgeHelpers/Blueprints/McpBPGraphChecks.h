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

// Whether a variable Get or Set node acts on the Blueprint's own object: one of its variables, its target pin unwired.
// A Set whose target is a spawned copy writes that copy's variable, not this one's.
bool McpVariableOnSelf(const UEdGraphNode* Node);

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

// An event's own values (its parameters, a Cast's result, a spawned actor) live in one slot each, and every run of the
// event writes them anew. Read after a wait (a Delay, a timeline, an async task's later output), they hold whatever
// the latest run wrote: a coin payout read its cast after a two-second delay, a second overlap failed that cast, and
// every payout read None. A variable the event sets on every run before the wait is no safer. A node behind a wait
// that reads what the event's every-run prefix writes, its outputs or its variable Sets, gets the warning.
void McpAddWaitRereadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
