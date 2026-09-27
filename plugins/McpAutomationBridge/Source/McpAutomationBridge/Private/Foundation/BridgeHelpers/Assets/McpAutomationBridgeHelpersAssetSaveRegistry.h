#pragma once

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreMinimal.h"
#include "Misc/PackageName.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

// A batch (build_graph) runs single-step handlers that each save the asset they
// edit, so a 40-step batch wrote the package 40 times, and each write was one
// more chance for a save to go wrong mid-batch. While a deferral is open those
// saves are skipped; the batch saves once when its steps are done. Not `static`:
// every translation unit must see the same counter.
inline int32 &McpAssetSaveDeferralDepth() {
  static int32 Depth = 0;
  return Depth;
}

struct FMcpDeferAssetSaves {
  FMcpDeferAssetSaves() { ++McpAssetSaveDeferralDepth(); }
  ~FMcpDeferAssetSaves() { --McpAssetSaveDeferralDepth(); }
};

/// Saves Asset's package through McpSafeAssetSave. A clean package whose file
// is already on disk has nothing to write, so it is skipped and answers true;
// a dirty package is unsaved work and is always written.
//
// bForce: save even inside a batch deferral or when the package is clean.
static inline bool SaveLoadedAssetThrottled(UObject *Asset, bool bForce = false) {
  if (!Asset)
    return false;
  if (!bForce && McpAssetSaveDeferralDepth() > 0)
    return true; // the batch that opened the deferral saves this asset once at its end
  const UPackage *const Package = Asset->GetOutermost();
  if (!bForce && Package && !Package->IsDirty() &&
      FPackageName::DoesPackageExist(Package->GetName()))
    return true;
  const bool bSaved = McpSafeAssetSave(Asset);
  if (!bSaved) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SaveLoadedAssetThrottled: failed to save '%s'"), *Asset->GetPathName());
  }
  return bSaved;
}

