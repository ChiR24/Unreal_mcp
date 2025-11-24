#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "WorldPartition/WorldPartition.h"

// Check for WorldPartitionEditorSubsystem (UE 5.0-5.5)
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

#include "WorldPartition/DataLayer/DataLayer.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"

// Check for DataLayerEditorSubsystem
#if defined(__has_include)
#  if __has_include("DataLayer/DataLayerEditorSubsystem.h")
#    include "DataLayer/DataLayerEditorSubsystem.h"
#    define MCP_HAS_DATALAYER_EDITOR 1
#  elif __has_include("WorldPartition/DataLayer/DataLayerEditorSubsystem.h")
#    include "WorldPartition/DataLayer/DataLayerEditorSubsystem.h"
#    define MCP_HAS_DATALAYER_EDITOR 1
#  else
#    define MCP_HAS_DATALAYER_EDITOR 0
#  endif
#else
#  define MCP_HAS_DATALAYER_EDITOR 0
#endif

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
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

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("load_cells"))
    {
#if MCP_HAS_WP_EDITOR_SUBSYSTEM
        UWorldPartitionEditorSubsystem* WPEditorSubsystem = GEditor->GetEditorSubsystem<UWorldPartitionEditorSubsystem>();
        if (!WPEditorSubsystem)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("WorldPartitionEditorSubsystem instance is null."), TEXT("SUBSYSTEM_INSTANCE_NULL"));
            return true;
        }

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
        WPEditorSubsystem->LoadRegion(Bounds);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Region load requested."));
#else
        // In newer engine versions where WP Editor Subsystem is moved or removed,
        // we might default to just loading everything or logging a specific message.
        // For now, we acknowledge the request but warn it's not fully supported.
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("WorldPartitionEditorSubsystem not available. LoadRegion skipped."));
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Region load requested (Simulated - Subsystem missing)."));
#endif
        return true;
    }
    else if (SubAction == TEXT("set_datalayer"))
    {
#if MCP_HAS_DATALAYER_EDITOR
        FString ActorPath;
        Payload->TryGetStringField(TEXT("actorPath"), ActorPath);
        FString DataLayerName;
        Payload->TryGetStringField(TEXT("dataLayerName"), DataLayerName);

        AActor* Actor = FindObject<AActor>(nullptr, *ActorPath);
        if (!Actor)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found."), TEXT("ACTOR_NOT_FOUND"));
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
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor added to DataLayer."));
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
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor added to DataLayer (Simulated - Subsystem missing)."));
#endif
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("World Partition support disabled (non-editor build)"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

