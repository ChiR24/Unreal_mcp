#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;

// The Blueprint's SCS components (name, class, parent) as the snapshot's components (Queries/McpBPSnapshotComponents.cpp).
void McpAppendBlueprintScsComponents(UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Snapshot);

namespace McpBlueprintHandlers {
// A Blueprint described for blueprint_get and the inspect tool (Blueprint/Queries/McpBPSnapshot.cpp).
TSharedPtr<FJsonObject> FEngineWire_BuildBlueprintSnapshot(UBlueprint *Blueprint, const FString &NormalizedPath);
} // namespace McpBlueprintHandlers
