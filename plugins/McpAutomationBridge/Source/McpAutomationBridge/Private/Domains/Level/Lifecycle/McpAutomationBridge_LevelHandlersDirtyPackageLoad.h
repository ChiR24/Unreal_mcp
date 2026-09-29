#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UPackage;

namespace McpLevelHandlers {
void CountBlockingDirtyPackages(int32& OutWorldPackages, int32& OutContentPackages);
/** unsaved (LevelPackage is dirty), unsavedPackages (the level and asset packages a level load would prompt for, first 100) and unsavedPackageCount. */
void AddUnsavedState(const TSharedPtr<FJsonObject>& Result, const UPackage* LevelPackage);
bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages, int32& OutInitialContentPackages, int32& OutRemainingWorldPackages, int32& OutRemainingContentPackages, int32& OutFailedPackages);
} // namespace McpLevelHandlers
