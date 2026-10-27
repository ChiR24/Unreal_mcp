#pragma once

#include "CoreMinimal.h"

class UPackage;

namespace McpLevelHandlers {
int32 RemoveStreamingReferencesForLevelDelete(const FString& LongPackageName, const FString& ObjectPath, bool& bCurrentWorldMatchesTarget);
void TryUnloadLoadedLevelPackageForDelete(const FString& LongPackageName, bool bCurrentWorldMatchesTarget, UPackage*& LoadedPackage, bool& bPackageUnloadAttempted, bool& bPackageUnloadSucceeded);

// What DeleteLevelFiles found on disk and removed.
struct FLevelFileDeletion {
  FString MapFilename;
  bool bMapExisted = false;
  bool bDeletedMap = false;
  bool bBuiltDataExists = false;
  bool bDeletedBuiltData = false;
  bool bSidecarDeleteAttempted = false;
  bool bExternalActorsExists = false;
  bool bDeletedExternalActors = false;
  bool bExternalObjectsExists = false;
  bool bDeletedExternalObjects = false;
  // The first external-directory refusal; empty when none failed.
  FString SidecarErrorMessage;
  FString SidecarErrorCode;

  bool SidecarsRemoved() const {
    return SidecarErrorMessage.IsEmpty() && (!bBuiltDataExists || bDeletedBuiltData) &&
           (!bExternalActorsExists || bDeletedExternalActors) &&
           (!bExternalObjectsExists || bDeletedExternalObjects);
  }
};

// Finds PackagePath's map file and _BuiltData package; with bDelete, removes them and, once the map
// file is gone, its __ExternalActors__/__ExternalObjects__ directories.
void DeleteLevelFiles(const FString& PackagePath, bool bDelete, FLevelFileDeletion& Out);
} // namespace McpLevelHandlers
