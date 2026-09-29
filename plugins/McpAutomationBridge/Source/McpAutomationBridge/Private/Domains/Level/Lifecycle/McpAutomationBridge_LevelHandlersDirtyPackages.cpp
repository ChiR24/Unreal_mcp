#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "RenderingThread.h"

#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsLevelSave.h"

using McpSafeOperations::McpSafeAssetSave;
using McpSafeOperations::McpSafeLevelSave;

namespace McpLevelHandlers {
bool IsBlockingDirtyPackageForLevelLoad(UPackage* Package) {
  if (!Package || Package->HasAnyFlags(RF_Transient)) {
    return false;
  }

  const FString PackagePath = Package->GetPathName();
  return !PackagePath.StartsWith(TEXT("/Temp/")) &&
         !PackagePath.StartsWith(TEXT("/Transient/")) &&
         !PackagePath.StartsWith(TEXT("/Engine/Transient"));
}

namespace {
// The dirty packages that block a level load, each once: world packages first, OutWorldCount of them.
TArray<UPackage*> CollectBlockingDirtyPackages(int32& OutWorldCount) {
  TArray<UPackage*> WorldPackages;
  TArray<UPackage*> ContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(WorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(ContentPackages);
  TArray<UPackage*> Blocking;
  for (UPackage* Package : WorldPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package)) {
      Blocking.AddUnique(Package);
    }
  }
  OutWorldCount = Blocking.Num();
  for (UPackage* Package : ContentPackages) {
    if (IsBlockingDirtyPackageForLevelLoad(Package)) {
      Blocking.AddUnique(Package);
    }
  }
  return Blocking;
}
} // namespace

void CountBlockingDirtyPackages(int32& OutWorldPackages,
                                int32& OutContentPackages) {
  OutContentPackages = CollectBlockingDirtyPackages(OutWorldPackages).Num() - OutWorldPackages;
}

void AddUnsavedState(const TSharedPtr<FJsonObject>& Result, const UPackage* LevelPackage) {
  // "Is the level saved?" had no read answer; restart_editor validateOnly was the only one.
  int32 WorldCount = 0;
  const TArray<UPackage*> Packages = CollectBlockingDirtyPackages(WorldCount);
  TArray<TSharedPtr<FJsonValue>> Names;
  for (int32 Index = 0; Index < Packages.Num() && Index < 100; ++Index) {
    Names.Add(MakeShared<FJsonValueString>(Packages[Index]->GetName()));
  }
  Result->SetBoolField(TEXT("unsaved"), LevelPackage && LevelPackage->IsDirty());
  Result->SetArrayField(TEXT("unsavedPackages"), Names);
  Result->SetNumberField(TEXT("unsavedPackageCount"), Packages.Num());
}

bool SaveBlockingDirtyPackagesForLevelLoad(int32& OutInitialWorldPackages,
                                           int32& OutInitialContentPackages,
                                           int32& OutRemainingWorldPackages,
                                           int32& OutRemainingContentPackages,
                                           int32& OutFailedPackages) {
  OutFailedPackages = 0;
  const TArray<UPackage*> Packages = CollectBlockingDirtyPackages(OutInitialWorldPackages);
  OutInitialContentPackages = Packages.Num() - OutInitialWorldPackages;
  for (UPackage* Package : Packages) {
    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    const bool bSaved = PackageWorld && PackageWorld->PersistentLevel
        ? McpSafeLevelSave(PackageWorld->PersistentLevel, Package->GetName())
        : McpSafeAssetSave(Package);
    OutFailedPackages += bSaved ? 0 : 1;
  }

  FlushRenderingCommands();
  CountBlockingDirtyPackages(OutRemainingWorldPackages, OutRemainingContentPackages);
  return OutFailedPackages == 0 && OutRemainingWorldPackages + OutRemainingContentPackages == 0;
}
} // namespace McpLevelHandlers
