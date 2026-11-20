#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// The runtime reads, on the game thread: runtime.list_actors {classFilter?, limit?}, runtime.get_transform
// {actorName}, runtime.frame_cost {} and runtime.screenshot {}. Null with OutError set for an unknown capability, a
// missing actor or no world.
TSharedPtr<FJsonObject> McpRuntimeRead(const FString& Capability, const TSharedPtr<FJsonObject>& Params, FString& OutError);

// The capability names McpRuntimeRead takes, in order.
const TArray<FString>& McpRuntimeCapabilities();
