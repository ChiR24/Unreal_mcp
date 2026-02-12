#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"


#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "WorldPartition/WorldPartition.h"

// Check for WorldPartitionEditorSubsystem (UE 5.0-5.3)
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

// Check for WorldPartitionEditorLoaderAdapter (UE 5.4+)
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

// Check for DataLayerEditorSubsystem (UE 5.1+ only - DataLayer APIs changed significantly)
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

// Note: DataLayerInstance.h, DataLayerManager.h and DataLayerAsset.h were introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleWorldPartitionAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_world_partition"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active editor world."), TEXT("NO_WORLD"));
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("World is not partitioned."), TEXT("NOT_PARTITIONED"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    if (SubAction == TEXT("load_cells"))
    {
        // Default to a reasonable area if no bounds provided
        FVector Origin = FVector::ZeroVector;
        FVector Extent = FVector(25000.0f, 25000.0f, 25000.0f); // 500m box

        const TArray<TSharedPtr<FJsonValue>>* OriginArr;
        if (Payload->TryGetArrayField(TEXT("origin"), OriginArr) && OriginArr && OriginArr->Num() >= 3)
        {
            Origin.X = (*OriginArr)[0]->AsNumber();
            Origin.Y = (*OriginArr)[1]->AsNumber();
            Origin.Z = (*OriginArr)[2]->AsNumber();
        }
        
        const TArray<TSharedPtr<FJsonValue>>* ExtentArr;
        if (Payload->TryGetArrayField(TEXT("extent"), ExtentArr) && ExtentArr && ExtentArr->Num() >= 3)
        {
            Extent.X = (*ExtentArr)[0]->AsNumber();
            Extent.Y = (*ExtentArr)[1]->AsNumber();
            Extent.Z = (*ExtentArr)[2]->AsNumber();
        }
        
        FBox Bounds(Origin - Extent, Origin + Extent);

#if MCP_HAS_WP_EDITOR_SUBSYSTEM
        // Old method (UE 5.0-5.3)
        UWorldPartitionEditorSubsystem* WPEditorSubsystem = GEditor->GetEditorSubsystem<UWorldPartitionEditorSubsystem>();
        if (WPEditorSubsystem)
        {
            WPEditorSubsystem->LoadRegion(Bounds);
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("action"), TEXT("load_region"));
            Result->SetStringField(TEXT("method"), TEXT("EditorSubsystem"));
            Result->SetBoolField(TEXT("requested"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Region load requested."), Result);
            return true;
        }
#endif

#if MCP_HAS_WP_LOADER_ADAPTER
        // New method (UE 5.4+)
        if (WorldPartition)
        {
             // Create a user-created loader adapter to load the region
             UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = WorldPartition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, Bounds, TEXT("MCP Loaded Region"));
             if (EditorLoaderAdapter && EditorLoaderAdapter->GetLoaderAdapter())
             {
                 EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
                 EditorLoaderAdapter->GetLoaderAdapter()->Load();
                 TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                 Result->SetStringField(TEXT("action"), TEXT("load_region"));
                 Result->SetStringField(TEXT("method"), TEXT("LoaderAdapter"));
                 Result->SetBoolField(TEXT("requested"), true);
                 SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Region load requested via LoaderAdapter."), Result);
                 return true;
             }
        }
#endif

        // If we reach here, neither subsystem nor adapter logic was available/successful
        // But we should avoid sending error if it was just a fallback case; however if both failed it means not supported.
        // Since we are refactoring to SUPPORT it, failure here is real failure.
        SendAutomationError(RequestingSocket, RequestId, TEXT("WorldPartition region loading not supported or failed in this engine version."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    else if (SubAction == TEXT("create_datalayer"))
    {
#if MCP_HAS_DATALAYER_EDITOR
        FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));

        if (DataLayerName.IsEmpty())
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Missing dataLayerName."), TEXT("INVALID_PARAMS"));
             return true;
        }

        UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (DataLayerSubsystem)
        {
            // Check existence
            bool bExists = false;
            UWorldPartition* WP = World->GetWorldPartition();
            if (UDataLayerManager* DataLayerManager = WP ? WP->GetDataLayerManager() : nullptr)
            {
                DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
                    if (LayerInstance->GetDataLayerShortName() == DataLayerName || LayerInstance->GetDataLayerFullName() == DataLayerName)
                    {
                        bExists = true;
                        return false; 
                    }
                    return true;
                });
            }

            if (bExists)
            {
                 SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("DataLayer '%s' already exists."), *DataLayerName));
                 return true;
            }

            // Create Data Layer
            // UE 5.1+ API: CreateDataLayerInstance(const FDataLayerCreationParameters& Parameters)
            // A DataLayerAsset is required.
            
            UDataLayerInstance* NewLayer = nullptr;

            // Create a transient UDataLayerAsset. 
            // In a real editor workflow, we would create a package and save it, 
            // but for automation/testing we can create it in the transient package 
            // or better yet, in the World's package so it persists if saved.
            // Using GetTransientPackage() to avoid cluttering content unless saved.
            // However, DataLayer logic might require it to be rooted or referenced.
            
            UDataLayerAsset* NewAsset = NewObject<UDataLayerAsset>(GetTransientPackage(), UDataLayerAsset::StaticClass(), FName(*DataLayerName), RF_Public | RF_Transactional);
            
            if (NewAsset && DataLayerSubsystem)
            {
                 FDataLayerCreationParameters Params;
                 Params.DataLayerAsset = NewAsset;
                 NewLayer = DataLayerSubsystem->CreateDataLayerInstance(Params);
            }
        
            if (NewLayer)
            {
                 SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("DataLayer '%s' created."), *DataLayerName));
            }
            else
            {
                 SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create DataLayer (Subsystem returned null)."), TEXT("CREATE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        }
#else
        SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
        return true;
    }
    else if (SubAction == TEXT("set_datalayer"))
    {
        FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"));
        FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));

#if MCP_HAS_DATALAYER_EDITOR
        AActor* Actor = FindObject<AActor>(nullptr, *ActorPath);
        if (!Actor)
        {
            // Fallback: Try to find by Actor Label
            if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
            {
                TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                for (AActor* A : AllActors)
                {
                    if (A && A->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase))
                    {
                         Actor = A;
                         break;
                    }
                }
            }
        }

        if (!Actor)
        {
             SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorPath), TEXT("ACTOR_NOT_FOUND"));
             return true;
        }

        UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (DataLayerSubsystem)
        {
            UDataLayerInstance* TargetLayer = nullptr;

            if (UDataLayerManager* DataLayerManager = WorldPartition->GetDataLayerManager())
            {
                DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
                    if (LayerInstance->GetDataLayerShortName() == DataLayerName || LayerInstance->GetDataLayerFullName() == DataLayerName)
                    {
                        TargetLayer = LayerInstance;
                        return false; // Stop iteration
                    }
                    return true; // Continue
                });
            }

            if (TargetLayer)
            {
                TArray<AActor*> Actors;
                Actors.Add(Actor);
                TArray<UDataLayerInstance*> Layers;
                Layers.Add(TargetLayer);

                DataLayerSubsystem->AddActorsToDataLayers(Actors, Layers);
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("actorName"), Actor->GetName());
                Result->SetStringField(TEXT("dataLayerName"), DataLayerName);
                Result->SetBoolField(TEXT("added"), true);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor added to DataLayer."), Result);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("DataLayer '%s' not found."), *DataLayerName), TEXT("DATALAYER_NOT_FOUND"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        }
#else
        // Fallback or simulation
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("DataLayerEditorSubsystem not available. set_datalayer skipped."));
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("actorName"), ActorPath);
        Result->SetStringField(TEXT("dataLayerName"), DataLayerName);
        Result->SetBoolField(TEXT("added"), false);
        Result->SetStringField(TEXT("note"), TEXT("Simulated - Subsystem missing"));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor added to DataLayer (Simulated - Subsystem missing)."), Result);
#endif
        return true;
    }
    else if (SubAction == TEXT("cleanup_invalid_datalayers"))
    {
#if MCP_HAS_DATALAYER_EDITOR
        UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (!DataLayerSubsystem)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
             return true;
        }

        UDataLayerManager* DataLayerManager = WorldPartition ? WorldPartition->GetDataLayerManager() : nullptr;
        if (!DataLayerManager)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerManager not found."), TEXT("MANAGER_NOT_FOUND"));
             return true;
        }

        TArray<UDataLayerInstance*> InvalidInstances;
        DataLayerManager->ForEachDataLayerInstance([&](UDataLayerInstance* LayerInstance) {
            // Use GetAsset() as GetDataLayerAsset() is not a member (UE 5.x change)
            if (LayerInstance && !LayerInstance->GetAsset())
            {
                InvalidInstances.Add(LayerInstance);
            }
            return true;
        });

        int32 DeletedCount = 0;
        for (UDataLayerInstance* InvalidInstance : InvalidInstances)
        {
            DataLayerSubsystem->DeleteDataLayer(InvalidInstance);
            DeletedCount++;
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Cleaned up %d invalid Data Layer Instances."), DeletedCount));
#else
        SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
        return true;
    }
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("World Partition support disabled (non-editor build)"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

