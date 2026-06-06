#pragma once

#include "CoreMinimal.h"

class UPackage;

namespace McpLevelHandlers {
#if WITH_EDITOR
int32 RemoveStreamingReferencesForLevelDelete(const FString& LongPackageName, const FString& ObjectPath, bool& bCurrentWorldMatchesTarget);
void TryUnloadLoadedLevelPackageForDelete(const FString& LongPackageName, bool bCurrentWorldMatchesTarget, UPackage*& LoadedPackage, bool& bPackageUnloadAttempted, bool& bPackageUnloadSucceeded);
void RescanLevelPackageForDelete(const FString& LongPackageName, bool bHasMapFilename, const FString& AbsoluteMapFilename);
#endif
} // namespace McpLevelHandlers
