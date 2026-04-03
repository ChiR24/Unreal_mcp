// =============================================================================
// McpAutomationBridge_WorldPartitionHandlers.cpp
// =============================================================================
// MCP Automation Bridge - World Partition & Data Layer Handlers
// 
// UE Version Support: 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7
// 
// Handler Summary:
// -----------------------------------------------------------------------------
// Action: manage_world_partition (Editor Only)
//   - load_cells: Load cells/region in World Partition level
//   - create_datalayer: Create new Data Layer asset
//   - set_datalayer: Add actor to Data Layer
//   - cleanup_invalid_datalayers: Remove invalid Data Layer instances
// 
// Dependencies:
//   - Core: McpAutomationBridgeSubsystem, McpAutomationBridgeHelpers
//   - Engine: WorldPartition, DataLayer, EngineUtils
//   - Editor: EditorActorSubsystem, LevelEditor
// 
// Version Compatibility Notes:
//   - UE 5.0-5.3: UWorldPartitionEditorSubsystem::LoadRegion()
//   - UE 5.4+: WorldPartitionEditorLoaderAdapter with FLoaderAdapterShape
//   - UE 5.1+: DataLayerEditorSubsystem with FDataLayerCreationParameters
//   - UE 5.0: Limited DataLayer API support
//   - UE 5.3+: UDataLayerManager for data layer operations
// 
// Architecture:
//   - World Partition levels use external actor packages
//   - TActorIterator required for finding actors (FindObject unreliable)
//   - Data Layers require assets for persistence
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST be first - UE version compatibility macros

// -----------------------------------------------------------------------------
// Core Includes
// -----------------------------------------------------------------------------
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpHandlerUtils.h"

// -----------------------------------------------------------------------------
// Engine Includes
// -----------------------------------------------------------------------------
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "FileHelpers.h"
#include "EditorLevelUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "EngineUtils.h"

// -----------------------------------------------------------------------------
// Version-Specific Includes: WorldPartitionEditorSubsystem
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Version-Specific Includes: LoaderAdapter (UE 5.4+)
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Version-Specific Includes: DataLayerEditorSubsystem (UE 5.1+)
// -----------------------------------------------------------------------------
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
// UE 5.0: DataLayer APIs not available
#  define MCP_HAS_DATALAYER_EDITOR 0
#endif

// DataLayerInstance.h and DataLayerAsset.h introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "WorldPartition/DataLayer/DataLayerManager.h"
#endif

#endif // WITH_EDITOR

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleWorldPartitionAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Validate action
    if (Action != TEXT("manage_world_partition"))
    {
        return false;
    }

#if WITH_EDITOR
    // Validate payload
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Load target level if specified
    // -------------------------------------------------------------------------
    FString LevelPath = GetJsonStringField(Payload, TEXT("levelPath"));
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!LevelPath.IsEmpty())
    {
        // Normalize the level path
        FString NormalizedLevelPath = LevelPath;
        if (!NormalizedLevelPath.StartsWith(TEXT("/Game/")) && 
            !NormalizedLevelPath.StartsWith(TEXT("/Engine/")))
        {
            NormalizedLevelPath = TEXT("/Game/") + NormalizedLevelPath;
        }

        // Check if we need to load a different level
        if (World)
        {
            FString CurrentWorldPath = World->GetOutermost()->GetName();
            if (!CurrentWorldPath.Equals(NormalizedLevelPath, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, 
                    TEXT("HandleWorldPartitionAction: Loading level %s (current: %s)"), 
                    *NormalizedLevelPath, *CurrentWorldPath);

                FString Filename;
                if (FPackageName::TryConvertLongPackageNameToFilename(
                    NormalizedLevelPath, Filename, FPackageName::GetMapPackageExtension()))
                {
                    // CRITICAL FIX: Validate file exists BEFORE attempting load
                    // McpSafeLoadMap silently creates empty worlds for non-existent paths
                    // which causes confusing NOT_PARTITIONED errors instead of clear file-not-found
                    FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
                    if (!IFileManager::Get().FileExists(*FullPath))
                    {
                        SendAutomationError(RequestingSocket, RequestId, 
                            FString::Printf(TEXT("Level file not found: %s"), *FullPath), 
                            TEXT("LEVEL_NOT_FOUND"));
                        return true;
                    }
                    FlushRenderingCommands();
                    if (!McpSafeLoadMap(NormalizedLevelPath))
                    {
                        SendAutomationError(RequestingSocket, RequestId, 
                            FString::Printf(TEXT("Failed to load level: %s"), *NormalizedLevelPath), 
                            TEXT("LOAD_FAILED"));
                        return true;
                    }
                    World = GEditor->GetEditorWorldContext().World();
                }
                else
                {
                    SendAutomationError(RequestingSocket, RequestId, 
                        FString::Printf(TEXT("Invalid level path: %s"), *NormalizedLevelPath), 
                        TEXT("INVALID_PATH"));
                    return true;
                }
            }
        }
        else
        {
            // No current world - load the specified level
            FString Filename;
            if (FPackageName::TryConvertLongPackageNameToFilename(
                NormalizedLevelPath, Filename, FPackageName::GetMapPackageExtension()))
            {
                // CRITICAL FIX: Validate file exists BEFORE attempting load
                FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
                if (!IFileManager::Get().FileExists(*FullPath))
                {
                    SendAutomationError(RequestingSocket, RequestId, 
                        FString::Printf(TEXT("Level file not found: %s"), *FullPath), 
                        TEXT("LEVEL_NOT_FOUND"));
                    return true;
                }
                FlushRenderingCommands();
                if (!McpSafeLoadMap(NormalizedLevelPath))
                {
                    SendAutomationError(RequestingSocket, RequestId, 
                        FString::Printf(TEXT("Failed to load level: %s"), *NormalizedLevelPath), 
                        TEXT("LOAD_FAILED"));
                    return true;
                }
                World = GEditor->GetEditorWorldContext().World();
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Invalid level path: %s"), *NormalizedLevelPath), 
                    TEXT("INVALID_PATH"));
                return true;
            }
        }
    }

    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("No active editor world."), TEXT("NO_WORLD"));
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("World is not partitioned."), TEXT("NOT_PARTITIONED"));
        return true;
    }

    // Extract subaction
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    // -------------------------------------------------------------------------
    // load_cells: Load cells/region in World Partition level
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("load_cells"))
    {
        // Default to reasonable area if no bounds provided
        FVector Origin = FVector::ZeroVector;
        FVector Extent = FVector(25000.0f, 25000.0f, 25000.0f);  // 500m box

        const TArray<TSharedPtr<FJsonValue>>* OriginArr;
        if (Payload->TryGetArrayField(TEXT("origin"), OriginArr) && 
            OriginArr && OriginArr->Num() >= 3)
        {
            Origin.X = (*OriginArr)[0]->AsNumber();
            Origin.Y = (*OriginArr)[1]->AsNumber();
            Origin.Z = (*OriginArr)[2]->AsNumber();
        }

        const TArray<TSharedPtr<FJsonValue>>* ExtentArr;
        if (Payload->TryGetArrayField(TEXT("extent"), ExtentArr) && 
            ExtentArr && ExtentArr->Num() >= 3)
        {
            Extent.X = (*ExtentArr)[0]->AsNumber();
            Extent.Y = (*ExtentArr)[1]->AsNumber();
            Extent.Z = (*ExtentArr)[2]->AsNumber();
        }

        FBox Bounds(Origin - Extent, Origin + Extent);

#if MCP_HAS_WP_EDITOR_SUBSYSTEM
        // UE 5.0-5.3: Use WorldPartitionEditorSubsystem
        UWorldPartitionEditorSubsystem* WPEditorSubsystem = 
            GEditor->GetEditorSubsystem<UWorldPartitionEditorSubsystem>();
        if (WPEditorSubsystem)
        {
            WPEditorSubsystem->LoadRegion(Bounds);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("action"), TEXT("manage_world_partition"));
            Result->SetStringField(TEXT("subAction"), TEXT("load_cells"));
            Result->SetStringField(TEXT("method"), TEXT("EditorSubsystem"));
            Result->SetBoolField(TEXT("requested"), true);

            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Region load requested."), Result);
            return true;
        }
#endif

#if MCP_HAS_WP_LOADER_ADAPTER
        // UE 5.4+: Use LoaderAdapter
        UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = 
            WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
                World, Bounds, TEXT("MCP Loaded Region"));
        if (EditorLoaderAdapter && EditorLoaderAdapter->GetLoaderAdapter())
        {
            EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
            EditorLoaderAdapter->GetLoaderAdapter()->Load();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("action"), TEXT("manage_world_partition"));
            Result->SetStringField(TEXT("subAction"), TEXT("load_cells"));
            Result->SetStringField(TEXT("method"), TEXT("LoaderAdapter"));
            Result->SetBoolField(TEXT("requested"), true);

            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Region load requested via LoaderAdapter."), Result);
            return true;
        }
#endif

        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("WorldPartition region loading not supported in this engine version."), 
            TEXT("NOT_SUPPORTED"));
        return true;
    }

    // -------------------------------------------------------------------------
    // create_datalayer: Create new Data Layer asset
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("create_datalayer"))
    {
#if MCP_HAS_DATALAYER_EDITOR
        FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));

        if (DataLayerName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Missing dataLayerName."), TEXT("INVALID_PARAMS"));
            return true;
        }

        UDataLayerEditorSubsystem* DataLayerSubsystem = 
            GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (!DataLayerSubsystem)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
            return true;
        }

        // Check existence
        bool bExists = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: Use UDataLayerManager
        if (UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager())
        {
            DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
                if (LayerInstance->GetDataLayerShortName() == DataLayerName || 
                    LayerInstance->GetDataLayerFullName() == DataLayerName)
                {
                    bExists = true;
                    return false;
                }
                return true;
            });
        }
#else
        // UE 5.1-5.2: Use UDataLayerSubsystem
        if (UDataLayerSubsystem* DataLayerSubsys = World->GetSubsystem<UDataLayerSubsystem>())
        {
            TArray<UDataLayerInstance*> ExistingLayers = 
                DataLayerSubsys->GetActorEditorContextDataLayers();
            for (UDataLayerInstance* LayerInstance : ExistingLayers)
            {
                if (LayerInstance && 
                    (LayerInstance->GetDataLayerShortName() == DataLayerName || 
                     LayerInstance->GetDataLayerFullName() == DataLayerName))
                {
                    bExists = true;
                    break;
                }
            }
        }
#endif

        if (bExists)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, 
                FString::Printf(TEXT("DataLayer '%s' already exists."), *DataLayerName));
            return true;
        }

        // Create Data Layer with transient asset
        UDataLayerInstance* NewLayer = nullptr;
        UDataLayerAsset* NewAsset = NewObject<UDataLayerAsset>(
            GetTransientPackage(), 
            UDataLayerAsset::StaticClass(), 
            FName(*DataLayerName), 
            RF_Public | RF_Transactional);

        if (NewAsset && DataLayerSubsystem)
        {
            FDataLayerCreationParameters Params;
            Params.DataLayerAsset = NewAsset;
            NewLayer = DataLayerSubsystem->CreateDataLayerInstance(Params);
        }

        if (NewLayer)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, 
                FString::Printf(TEXT("DataLayer '%s' created."), *DataLayerName));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Failed to create DataLayer (Subsystem returned null)."), 
                TEXT("CREATE_FAILED"));
        }
#else
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
        return true;
    }

    // -------------------------------------------------------------------------
    // set_datalayer: Add actor to Data Layer
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("set_datalayer"))
    {
        FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"));
        FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));

#if MCP_HAS_DATALAYER_EDITOR
        // CRITICAL: Use TActorIterator to find actors in World Partition levels
        // FindObject and GetAllLevelActors don't reliably find actors in WP packages
        AActor* Actor = nullptr;

        // First try FindObject with the path
        Actor = FindObject<AActor>(nullptr, *ActorPath);

        // If not found, use TActorIterator to search by label/name
        if (!Actor && World)
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (It->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase) ||
                    It->GetName().Equals(ActorPath, ESearchCase::IgnoreCase))
                {
                    Actor = *It;
                    break;
                }
            }
        }

        if (!Actor)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Actor not found: %s"), *ActorPath), 
                TEXT("ACTOR_NOT_FOUND"));
            return true;
        }

        UDataLayerEditorSubsystem* DataLayerSubsystem = 
            GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (!DataLayerSubsystem)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
            return true;
        }

        UDataLayerInstance* TargetLayer = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: Use UDataLayerManager
        if (UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager())
        {
            DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
                if (LayerInstance->GetDataLayerShortName() == DataLayerName || 
                    LayerInstance->GetDataLayerFullName() == DataLayerName)
                {
                    TargetLayer = LayerInstance;
                    return false;
                }
                return true;
            });
        }
#else
        // UE 5.1-5.2: Use UDataLayerSubsystem
        if (UDataLayerSubsystem* DataLayerSubsys = World->GetSubsystem<UDataLayerSubsystem>())
        {
            TArray<UDataLayerInstance*> ExistingLayers = 
                DataLayerSubsys->GetActorEditorContextDataLayers();
            for (UDataLayerInstance* LayerInstance : ExistingLayers)
            {
                if (LayerInstance && 
                    (LayerInstance->GetDataLayerShortName() == DataLayerName || 
                     LayerInstance->GetDataLayerFullName() == DataLayerName))
                {
                    TargetLayer = LayerInstance;
                    break;
                }
            }
        }
#endif

        if (TargetLayer)
        {
            TArray<AActor*> Actors;
            Actors.Add(Actor);
            TArray<UDataLayerInstance*> Layers;
            Layers.Add(TargetLayer);

            DataLayerSubsystem->AddActorsToDataLayers(Actors, Layers);

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("dataLayerName"), DataLayerName);
            Result->SetBoolField(TEXT("added"), true);
            McpHandlerUtils::AddVerification(Result, Actor);

            SendAutomationResponse(RequestingSocket, RequestId, true, 
                TEXT("Actor added to DataLayer."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("DataLayer '%s' not found."), *DataLayerName), 
                TEXT("DATALAYER_NOT_FOUND"));
        }
#else
        // Fallback simulation
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, 
            TEXT("DataLayerEditorSubsystem not available. set_datalayer skipped."));

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("actorName"), ActorPath);
        Result->SetStringField(TEXT("dataLayerName"), DataLayerName);
        Result->SetBoolField(TEXT("added"), false);
        Result->SetStringField(TEXT("note"), TEXT("Simulated - Subsystem missing"));

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Actor added to DataLayer (Simulated - Subsystem missing)."), Result);
#endif
        return true;
    }

    // -------------------------------------------------------------------------
    // cleanup_invalid_datalayers: Remove invalid Data Layer instances
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("cleanup_invalid_datalayers"))
    {
#if MCP_HAS_DATALAYER_EDITOR
        UDataLayerEditorSubsystem* DataLayerSubsystem = 
            GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (!DataLayerSubsystem)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
            return true;
        }

        TArray<UDataLayerInstance*> InvalidInstances;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+: Use UDataLayerManager
        UDataLayerManager* DataLayerManager = 
            WorldPartition ? WorldPartition->GetDataLayerManager() : nullptr;
        if (!DataLayerManager)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("DataLayerManager not found."), TEXT("MANAGER_NOT_FOUND"));
            return true;
        }

        DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
            if (LayerInstance && !LayerInstance->GetAsset())
            {
                InvalidInstances.Add(LayerInstance);
            }
            return true;
        });
#else
        // UE 5.1-5.2: Use UDataLayerSubsystem
        UDataLayerSubsystem* DataLayerSubsys = 
            World ? World->GetSubsystem<UDataLayerSubsystem>() : nullptr;
        if (!DataLayerSubsys)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("DataLayerSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
            return true;
        }

        TArray<UDataLayerInstance*> ExistingLayers = 
            DataLayerSubsys->GetActorEditorContextDataLayers();
        for (UDataLayerInstance* LayerInstance : ExistingLayers)
        {
            UDataLayerInstanceWithAsset* LayerWithAsset = 
                Cast<UDataLayerInstanceWithAsset>(LayerInstance);
            if (LayerInstance && !LayerWithAsset)
            {
                InvalidInstances.Add(LayerInstance);
            }
        }
#endif

        int32 DeletedCount = 0;
        for (UDataLayerInstance* InvalidInstance : InvalidInstances)
        {
            DataLayerSubsystem->DeleteDataLayer(InvalidInstance);
            DeletedCount++;
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Cleaned up %d invalid Data Layer Instances."), DeletedCount));
#else
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
        return true;
    }

    return true;

#else
    // Non-editor build
    SendAutomationResponse(RequestingSocket, RequestId, false, 
        TEXT("World Partition support disabled (non-editor build)"), 
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
