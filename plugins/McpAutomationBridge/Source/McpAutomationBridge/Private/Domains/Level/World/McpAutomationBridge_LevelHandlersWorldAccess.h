#pragma once

#include "CoreMinimal.h"

class ULevel;
class UWorld;

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World);
} // namespace McpLevelHandlers
