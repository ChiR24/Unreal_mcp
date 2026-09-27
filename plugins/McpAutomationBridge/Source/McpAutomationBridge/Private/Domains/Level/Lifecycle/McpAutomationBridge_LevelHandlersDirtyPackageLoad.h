#pragma once

#include "CoreMinimal.h"

namespace McpLevelHandlers {
void CountBlockingDirtyPackages(int32& OutWorldPackages, int32& OutContentPackages);
bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages, int32& OutInitialContentPackages, int32& OutRemainingWorldPackages, int32& OutRemainingContentPackages, int32& OutFailedPackages);
} // namespace McpLevelHandlers
