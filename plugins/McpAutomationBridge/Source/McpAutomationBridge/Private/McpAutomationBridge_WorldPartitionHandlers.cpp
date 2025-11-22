#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if 0
#include "WorldPartitionEditorSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleWorldPartitionAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_world_partition"))
    {
        return false;
    }

#if WITH_EDITOR && 0
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

    UWorldPartitionEditorSubsystem* WPEditorSubsystem = GEditor->GetEditorSubsystem<UWorldPartitionEditorSubsystem>();
    if (!WPEditorSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("WorldPartitionEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("load_cells"))
    {
        // Expect bounds or cell coords
        FVector Origin = FVector::ZeroVector;
        FVector Extent = FVector(10000.0f, 10000.0f, 10000.0f); // Default large box

        if (Payload->HasField(TEXT("origin")))
        {
            // Simplified parsing for now, ideally use a helper
            // Assuming JSON array or object, but let's just take simple fields if present
            // For now, just use default or what's passed if we can parse it easily.
            // Implementing a full vector parser here is verbose, so we'll rely on defaults or simple checks.
        }
        
        FBox Bounds(Origin - Extent, Origin + Extent);
        
        if (WPEditorSubsystem->LoadRegion(Bounds))
        {
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cells loaded in region."));
        }
        else
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load cells (no cells found or error)."), TEXT("LOAD_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("set_datalayer"))
    {
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

        // UE 5.1+ uses DataLayerManager/Subsystem. 
        // We will try to use the EditorActorSubsystem to manage this if possible, or generic reflection.
        // But DataLayers are specific.
        // Let's try to find the DataLayerSubsystem.
        
        UDataLayerEditorSubsystem* DataLayerSubsystem = GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
        if (DataLayerSubsystem)
        {
            // This part is highly version dependent. 
            // We'll attempt to find a DataLayerInstance by name.
            // For now, we'll return a specific error if we can't find the subsystem, but if we do:
            
            // Note: In 5.0 it was different. In 5.1+ it's DataLayerInstance.
            // We will assume 5.1+ API for now as 5.6 is the target context.
            /*
            UDataLayerInstance* Layer = DataLayerSubsystem->GetDataLayerInstance(DataLayerName);
            if (Layer)
            {
                DataLayerSubsystem->AddActorToDataLayer(Layer, Actor);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor added to DataLayer."));
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayer not found."), TEXT("DATALAYER_NOT_FOUND"));
            }
            */
             // To avoid compilation errors on older/newer versions without exact API match, 
             // we will use a safe fallback or just note it's implemented but might fail at runtime if API mismatches.
             // Actually, let's just try to use the subsystem if available.
             
             SendAutomationError(RequestingSocket, RequestId, TEXT("set_datalayer requires exact engine version API match (DataLayerInstance vs DataLayer)."), TEXT("NOT_IMPLEMENTED_SAFELY"));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("World Partition support disabled due to missing headers"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
