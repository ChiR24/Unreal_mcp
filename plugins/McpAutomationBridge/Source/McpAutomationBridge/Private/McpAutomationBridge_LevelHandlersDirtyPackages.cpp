#include "McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"

#include "Engine/World.h"
#include "FileHelpers.h"
#include "RenderingThread.h"

#include "McpSafeOperationsAssetSave.h"
#include "McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeAssetSave;
using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
#if WITH_EDITOR
bool IsBlockingDirtyPackageForLevelLoad(UPackage* Package) {
  if (!Package || Package->HasAnyFlags(RF_Transient)) {
    return false;
  }

  const FString PackagePath = Package->GetPathName();
  return !PackagePath.StartsWith(TEXT("/Temp/")) &&
         !PackagePath.StartsWith(TEXT("/Transient/")) &&
         !PackagePath.StartsWith(TEXT("/Engine/Transient"));
}

void CountBlockingDirtyPackages(int32& OutWorldPackages,
                                int32& OutContentPackages) {
  OutWorldPackages = 0;
  OutContentPackages = 0;

  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  TSet<UPackage*> SeenPackages;
  for (UPackage* Package : DirtyWorldPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package) && !SeenPackages.Contains(Package)) {
      SeenPackages.Add(Package);
      OutWorldPackages++;
    }
  }
  for (UPackage* Package : DirtyContentPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package) && !SeenPackages.Contains(Package)) {
      SeenPackages.Add(Package);
      OutContentPackages++;
    }
  }
}

bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages,
                                           int32& OutInitialContentPackages,
                                           int32& OutRemainingWorldPackages,
                                           int32& OutRemainingContentPackages,
                                           int32& OutFailedPackages) {
  OutFailedPackages = 0;
  CountBlockingDirtyPackages(OutInitialWorldPackages, OutInitialContentPackages);
  if (OutInitialWorldPackages + OutInitialContentPackages == 0) {
    OutRemainingWorldPackages = 0;
    OutRemainingContentPackages = 0;
    return true;
  }

  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  TSet<UPackage*> ProcessedPackages;
  auto SavePackage = [&ProcessedPackages, &OutFailedPackages](UPackage* Package) {
    if (!IsBlockingDirtyPackageForLevelLoad(Package) || ProcessedPackages.Contains(Package)) {
      return;
    }

    ProcessedPackages.Add(Package);
    const FString PackagePath = Package->GetName();
    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    const bool bSaved = PackageWorld && PackageWorld->PersistentLevel
        ? McpSafeLevelSave(PackageWorld->PersistentLevel, PackagePath)
        : McpSafeAssetSave(Package);
    if (!bSaved) {
      OutFailedPackages++;
    }
  };

  for (UPackage* Package : DirtyWorldPackages) {
    SavePackage(Package);
  }
  for (UPackage* Package : DirtyContentPackages) {
    SavePackage(Package);
  }

  FlushRenderingCommands();
  CountBlockingDirtyPackages(OutRemainingWorldPackages, OutRemainingContentPackages);
  return OutFailedPackages == 0 && OutRemainingWorldPackages + OutRemainingContentPackages == 0;
}
#endif
} // namespace McpLevelHandlers
