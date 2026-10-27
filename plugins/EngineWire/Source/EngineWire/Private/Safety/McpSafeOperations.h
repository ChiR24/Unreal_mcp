#pragma once

#include "Safety/McpSafeOperationsAssetDelete.h"
#include "Safety/McpSafeOperationsAssetClassification.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsFolderDelete.h"
#include "Safety/McpSafeOperationsLevelSave.h"
#include "Safety/McpSafeOperationsMapLoad.h"
#include "Safety/McpSafeOperationsMaterial.h"
#include "Safety/McpSafeOperationsWorldDelete.h"

using McpSafeOperations::McpLoadMaterialWithFallback;
using McpSafeOperations::McpRefuseLoadOverUnsavedLevels;
using McpSafeOperations::McpSafeAssetSave;
using McpSafeOperations::McpSafeLevelSave;
using McpSafeOperations::McpSafeLoadMap;
using McpSafeOperations::ScanPathSynchronous;
