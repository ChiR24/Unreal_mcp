#pragma once

#include "CoreMinimal.h"

class ULevel;
class ULevelStreaming;
class UWorld;

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World);

/** The streaming levels whose package (PIE prefix removed) is Name or ends in /Name; an exact path wins outright. */
TArray<ULevelStreaming*> FindStreamingLevels(UWorld* World, const FString& Name);
} // namespace McpLevelHandlers
