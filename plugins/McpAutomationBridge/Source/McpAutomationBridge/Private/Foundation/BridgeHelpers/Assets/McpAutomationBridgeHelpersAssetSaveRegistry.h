#pragma once

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
// Throttled wrapper around McpSafeAssetSave to avoid triggering rapid repeated
// editor-owned package saves during heavy test activity. The helper consults a
// plugin-wide map of recent save timestamps (GRecentAssetSaveTs) and skips saves
// that occur within the configured throttle window. Skipped saves return 'true'
// to preserve idempotent behavior for callers that treat a skipped save as a
// success.
//
// bForce: If true, ignore throttling and force an immediate save.
static inline bool
SaveLoadedAssetThrottled(UObject *Asset, double ThrottleSecondsOverride = -1.0,
                         bool bForce = false) {
  if (!Asset)
    return false;
  const double Now = FPlatformTime::Seconds();
  const double Throttle = (ThrottleSecondsOverride >= 0.0)
                              ? ThrottleSecondsOverride
                              : GRecentAssetSaveThrottleSeconds;
  FString Key = Asset->GetPathName();
  if (Key.IsEmpty())
    Key = Asset->GetName();

  // Skipping is only safe when there is nothing to lose. The throttle exists to
  // stop redundant writes during heavy activity, but it used to skip on elapsed
  // time alone and still answer `true` -- so a caller editing one Blueprint in a
  // burst was told `saved: true` for edits that never reached disk, and lost the
  // tail of the burst on the next editor start with nothing in the reply saying
  // so. A dirty package is unsaved work by definition and is never redundant.
  const UPackage *const Package = Asset->GetOutermost();
  const bool bHasUnsavedWork = Package != nullptr && Package->IsDirty();

  {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    if (!bForce && !bHasUnsavedWork) {
      if (double *Last = GRecentAssetSaveTs.Find(Key)) {
        const double Elapsed = Now - *Last;
        if (Elapsed < Throttle) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
                 TEXT("SaveLoadedAssetThrottled: skipping save for '%s' "
                      "(clean, last=%.3fs, throttle=%.3fs)"),
                 *Key, Elapsed, Throttle);
          // Nothing to write: the asset on disk already matches memory.
          return true;
        }
      }
    }
  }

  // Perform the save through the UE 5.7-safe helper and record timestamp on success.
  const bool bSaved = McpSafeAssetSave(Asset);
  if (bSaved) {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    GRecentAssetSaveTs.Add(Key, Now);
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("SaveLoadedAssetThrottled: saved '%s' (throttle reset)"), *Key);
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SaveLoadedAssetThrottled: failed to save '%s'"), *Key);
  }
  return bSaved;
}

// Force a synchronous scan of a specific package or folder path to ensure
// the Asset Registry is up-to-date immediately after asset creation.
static inline void ScanPathSynchronous(const FString &InPath,
                                       bool bRecursive = true) {
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // Scan specific path
  TArray<FString> PathsToScan;
  PathsToScan.Add(InPath);
  AssetRegistry.ScanPathsSynchronous(PathsToScan, bRecursive);
}
#else
static inline bool
SaveLoadedAssetThrottled(void *Asset, double ThrottleSecondsOverride = -1.0,
                         bool bForce = false) {
  (void)Asset;
  (void)ThrottleSecondsOverride;
  (void)bForce;
  return false;
}
static inline void ScanPathSynchronous(const FString &InPath,
                                       bool bRecursive = true) {
  (void)InPath;
  (void)bRecursive;
}
#endif
