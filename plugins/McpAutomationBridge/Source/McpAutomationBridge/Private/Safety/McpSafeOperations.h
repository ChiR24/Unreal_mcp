#pragma once

#include "Safety/McpSafeOperationsAssetDelete.h"
#include "Safety/McpSafeOperationsAssetDeletePreparation.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsFolderDelete.h"
#include "Safety/McpSafeOperationsLevelSave.h"
#include "Safety/McpSafeOperationsMapLoad.h"
#include "Safety/McpSafeOperationsMaterial.h"
#include "Safety/McpSafeOperationsWorldDelete.h"

namespace McpSafeOperations
{

#if !WITH_EDITOR
inline bool McpSafeAssetSave(void* Asset) { return false; }
inline bool McpSafeLevelSave(void* Level, const FString& Path, int32 = 1) { return false; }
inline bool McpSafeLoadMap(const FString& MapPath, bool = true) { return false; }
inline class UMaterialInterface* McpLoadMaterialWithFallback(const FString& = FString(), bool = false) { return nullptr; }
inline void ScanPathSynchronous(const FString&, bool = true) {}
inline bool McpDeleteAssetAndFile(const FString&) { return false; }
#endif

}
