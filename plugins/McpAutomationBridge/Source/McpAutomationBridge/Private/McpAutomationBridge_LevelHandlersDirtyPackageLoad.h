#pragma once

#include "CoreMinimal.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
void CountBlockingDirtyPackages(int32& OutWorldPackages, int32& OutContentPackages);
bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages, int32& OutInitialContentPackages, int32& OutRemainingWorldPackages, int32& OutRemainingContentPackages, int32& OutFailedPackages);
#endif
} // namespace McpLevelHandlers
