#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class ULevel;
class UPackage;

namespace McpLevelHandlers {
void CountBlockingDirtyPackages(int32& OutWorldPackages, int32& OutContentPackages);
/** unsaved (the level's package or one of its external actor packages is dirty), unsavedPackages (the level and asset packages a level load would prompt for, first 100) and unsavedPackageCount. */
void AddUnsavedState(const TSharedPtr<FJsonObject>& Result, const ULevel* Level);
bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages, int32& OutInitialContentPackages, int32& OutRemainingWorldPackages, int32& OutRemainingContentPackages, int32& OutFailedPackages);
} // namespace McpLevelHandlers
