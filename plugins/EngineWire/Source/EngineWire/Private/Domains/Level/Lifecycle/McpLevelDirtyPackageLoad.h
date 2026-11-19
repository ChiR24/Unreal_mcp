#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class ULevel;
class UPackage;

namespace McpLevelHandlers {
/** unsavedPackages (the level and asset packages a level load would prompt for, first 100) and unsavedPackageCount; returns those packages. A DIRTY_PACKAGES refusal names what is left with it. */
TArray<UPackage*> AddBlockingDirtyPackages(const TSharedPtr<FJsonObject>& Result);
/** The first package's name, or "none". */
FString FirstPackageName(const TArray<UPackage*>& Packages);
/** unsaved (the level's package or one of its external actor packages is dirty), unsavedPackages (the level and asset packages a level load would prompt for, first 100) and unsavedPackageCount. */
void AddUnsavedState(const TSharedPtr<FJsonObject>& Result, const ULevel* Level);
bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages, int32& OutInitialContentPackages, int32& OutRemainingWorldPackages, int32& OutRemainingContentPackages, int32& OutFailedPackages);
} // namespace McpLevelHandlers
