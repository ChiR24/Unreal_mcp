#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;

namespace McpBlueprintHandlers {
// A Blueprint described for blueprint_get and the inspect tool (Blueprint/Events/McpBPEventIntrospection.cpp).
TSharedPtr<FJsonObject> FEngineWire_BuildBlueprintSnapshot(UBlueprint *Blueprint, const FString &NormalizedPath);
} // namespace McpBlueprintHandlers
