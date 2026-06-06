#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleWorldPartitionAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_world_partition"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    bool bLoadFailed = false;
    UWorld* World = McpWorldPartitionHandlers::LoadRequestedLevel(
        this, Payload, RequestingSocket, RequestId, bLoadFailed);
    if (bLoadFailed)
    {
        return true;
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

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction == TEXT("load_cells"))
    {
        return McpWorldPartitionHandlers::HandleLoadCells(
            this, RequestId, Payload, RequestingSocket, World, WorldPartition);
    }
    if (SubAction == TEXT("create_datalayer"))
    {
        return McpWorldPartitionHandlers::HandleCreateDataLayer(
            this, RequestId, Payload, RequestingSocket, World, WorldPartition);
    }
    if (SubAction == TEXT("set_datalayer"))
    {
        return McpWorldPartitionHandlers::HandleSetDataLayer(
            this, RequestId, Payload, RequestingSocket, World, WorldPartition);
    }
    if (SubAction == TEXT("cleanup_invalid_datalayers"))
    {
        return McpWorldPartitionHandlers::HandleCleanupInvalidDataLayers(
            this, RequestId, RequestingSocket, World, WorldPartition);
    }

    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("World Partition support disabled (non-editor build)"),
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
