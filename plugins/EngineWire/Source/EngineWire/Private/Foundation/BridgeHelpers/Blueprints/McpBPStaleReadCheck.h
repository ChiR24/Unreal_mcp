#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

// A pure node runs again for each node that reads it. One that reads a variable and feeds that variable's Set node
// also gives each node that runs after the Set the new value worked on again: Laps + 1 wired into Set Laps and into a
// Print after it printed Laps + 2, and a check against the goal passed a lap early. The compiler says nothing. One
// warning diagnostic per such pure node while Diagnostics holds fewer than MaxDiagnostics; the fix is the Set node's
// own output pin.
void McpAddStaleReadWarnings(const UBlueprint* Blueprint, TArray<TSharedPtr<FJsonValue>>& Diagnostics, int32 MaxDiagnostics);
