
#include "McpAutomationBridge_LevelHandlersDeletion.h"
#include "McpSafeOperationsPackageTools.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "McpAutomationBridgeLog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
namespace {
void FlushDeleteGarbage() {
  FlushRenderingCommands();
  if (GEditor) {
    GEditor->ForceGarbageCollection(true);
  }
  CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
  FlushRenderingCommands();
}
} // namespace

int32 RemoveStreamingReferencesForLevelDelete(const FString& LongPackageName,
                                              const FString& ObjectPath,
                                              bool& bCurrentWorldMatchesTarget) {
  int32 RemovedStreamingRefs = 0;
  bCurrentWorldMatchesTarget = false;
  if (!GEditor) {
    return RemovedStreamingRefs;
  }

  UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
  if (!EditorWorld) {
    return RemovedStreamingRefs;
  }

  bCurrentWorldMatchesTarget = EditorWorld->GetOutermost() &&
                               EditorWorld->GetOutermost()->GetName() == LongPackageName;
  if (bCurrentWorldMatchesTarget) {
    return RemovedStreamingRefs;
  }

  TArray<ULevelStreaming*> StreamingLevels = EditorWorld->GetStreamingLevels();
  for (ULevelStreaming* StreamingLevel : StreamingLevels) {
    if (!StreamingLevel) {
      continue;
    }

    const FString StreamingPackage = StreamingLevel->GetWorldAssetPackageFName().ToString();
    if (StreamingPackage != LongPackageName && StreamingPackage != ObjectPath) {
      continue;
    }

    StreamingLevel->SetShouldBeLoaded(false);
    StreamingLevel->SetShouldBeVisible(false);
    if (ULevel* LoadedStreamingLevel = StreamingLevel->GetLoadedLevel()) {
      if (UEditorLevelUtils::RemoveLevelFromWorld(LoadedStreamingLevel)) {
        ++RemovedStreamingRefs;
      }
    } else {
      EditorWorld->RemoveStreamingLevel(StreamingLevel);
      ++RemovedStreamingRefs;
    }
  }

  if (RemovedStreamingRefs > 0) {
    FlushDeleteGarbage();
  }
  return RemovedStreamingRefs;
}

void TryUnloadLoadedLevelPackageForDelete(const FString& LongPackageName,
                                          bool bCurrentWorldMatchesTarget,
                                          UPackage*& LoadedPackage,
                                          bool& bPackageUnloadAttempted,
                                          bool& bPackageUnloadSucceeded) {
  bPackageUnloadAttempted = false;
  bPackageUnloadSucceeded = false;
  if (!LoadedPackage || bCurrentWorldMatchesTarget) {
    return;
  }

  FlushDeleteGarbage();
  LoadedPackage = FindPackage(nullptr, *LongPackageName);
  if (!LoadedPackage) {
    return;
  }

#if MCP_HAS_PACKAGE_TOOLS
  TArray<UPackage*> PackagesToUnload;
  PackagesToUnload.Add(LoadedPackage);
  TWeakObjectPtr<UPackage> WeakLoadedPackage = LoadedPackage;
  FText UnloadError;
  bPackageUnloadAttempted = true;
  bPackageUnloadSucceeded = UPackageTools::UnloadPackages(PackagesToUnload, UnloadError, true);
  if (!UnloadError.IsEmpty()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("delete_level: UnloadPackages reported for %s: %s"),
           *LongPackageName, *UnloadError.ToString());
  }
  FlushDeleteGarbage();
  LoadedPackage = FindPackage(nullptr, *LongPackageName);
  bPackageUnloadSucceeded = bPackageUnloadSucceeded &&
      !WeakLoadedPackage.IsValid() && LoadedPackage == nullptr;
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("delete_level: PackageTools unavailable; cannot unload loaded map package %s"),
         *LongPackageName);
#endif
}

void RescanLevelPackageForDelete(const FString& LongPackageName,
                                 bool bHasMapFilename,
                                 const FString& AbsoluteMapFilename) {
  IAssetRegistry& AssetRegistry =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
  if (bHasMapFilename && IFileManager::Get().FileExists(*AbsoluteMapFilename)) {
    TArray<FString> FilesToScan;
    FilesToScan.Add(AbsoluteMapFilename);
    AssetRegistry.ScanFilesSynchronous(FilesToScan, true);
  }
  const FString PackageDir = FPaths::GetPath(LongPackageName);
  if (!PackageDir.IsEmpty()) {
    TArray<FString> PathsToScan;
    PathsToScan.Add(PackageDir);
    AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
  }
}
#endif
} // namespace McpLevelHandlers
