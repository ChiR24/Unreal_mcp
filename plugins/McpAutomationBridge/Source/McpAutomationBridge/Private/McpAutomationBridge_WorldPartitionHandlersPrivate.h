#pragma once

#include "McpVersionCompatibility.h"

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "LevelEditor.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "WorldPartition/WorldPartition.h"

#if defined(__has_include)
#  if __has_include("WorldPartition/WorldPartitionEditorSubsystem.h")
#    include "WorldPartition/WorldPartitionEditorSubsystem.h"
#    define MCP_HAS_WP_EDITOR_SUBSYSTEM 1
#  elif __has_include("WorldPartitionEditor/WorldPartitionEditorSubsystem.h")
#    include "WorldPartitionEditor/WorldPartitionEditorSubsystem.h"
#    define MCP_HAS_WP_EDITOR_SUBSYSTEM 1
#  else
#    define MCP_HAS_WP_EDITOR_SUBSYSTEM 0
#  endif
#else
#  define MCP_HAS_WP_EDITOR_SUBSYSTEM 0
#endif

#if defined(__has_include)
#  if __has_include("WorldPartition/WorldPartitionEditorLoaderAdapter.h")
#    include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"
#    include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#    define MCP_HAS_WP_LOADER_ADAPTER 1
#  else
#    define MCP_HAS_WP_LOADER_ADAPTER 0
#  endif
#else
#  define MCP_HAS_WP_LOADER_ADAPTER 0
#endif

#include "WorldPartition/DataLayer/DataLayer.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#  if defined(__has_include)
#    if __has_include("DataLayer/DataLayerEditorSubsystem.h")
#      include "DataLayer/DataLayerEditorSubsystem.h"
#      define MCP_HAS_DATALAYER_EDITOR 1
#    elif __has_include("WorldPartition/DataLayer/DataLayerEditorSubsystem.h")
#      include "WorldPartition/DataLayer/DataLayerEditorSubsystem.h"
#      define MCP_HAS_DATALAYER_EDITOR 1
#    else
#      define MCP_HAS_DATALAYER_EDITOR 0
#    endif
#  else
#    define MCP_HAS_DATALAYER_EDITOR 0
#  endif
#else
#  define MCP_HAS_DATALAYER_EDITOR 0
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/DataLayer/DataLayerManager.h"
#endif

namespace McpWorldPartitionHandlers
{
UWorld* LoadRequestedLevel(
    UMcpAutomationBridgeSubsystem* Bridge,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString& RequestId,
    bool& bOutFailed);

bool HandleLoadCells(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition);

bool HandleCreateDataLayer(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition);

bool HandleSetDataLayer(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition);

bool HandleCleanupInvalidDataLayers(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition);
}
#endif
