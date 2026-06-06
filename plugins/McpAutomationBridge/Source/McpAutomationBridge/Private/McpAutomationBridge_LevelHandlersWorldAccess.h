#pragma once

#include "CoreMinimal.h"

class ULevel;
class UWorld;

namespace McpLevelHandlers {
#if WITH_EDITOR
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World);
#endif
} // namespace McpLevelHandlers
