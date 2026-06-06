#pragma once

#include "McpSafeOperationsAssetDeletePreparation.h"
#include "McpSafeOperationsAssetSave.h"
#include "McpSafeOperationsFolderDelete.h"
#include "McpSafeOperationsLevelSave.h"
#include "McpSafeOperationsMapLoad.h"
#include "McpSafeOperationsMaterial.h"
#include "McpSafeOperationsWorldDelete.h"

namespace McpSafeOperations
{

#if !WITH_EDITOR
inline bool McpSafeAssetSave(void* Asset) { return false; }
inline bool McpSafeLevelSave(void* Level, const FString& Path, int32 = 1) { return false; }
inline bool McpSafeLoadMap(const FString& MapPath, bool = true) { return false; }
inline class UMaterialInterface* McpLoadMaterialWithFallback(const FString& = FString(), bool = false) { return nullptr; }
inline bool SaveLoadedAssetThrottled(void* Asset, double = -1.0, bool = false) { return false; }
inline void ScanPathSynchronous(const FString&, bool = true) {}
#endif

}
