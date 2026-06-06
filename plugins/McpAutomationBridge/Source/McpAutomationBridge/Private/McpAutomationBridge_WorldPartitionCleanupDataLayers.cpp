#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

#if WITH_EDITOR
namespace McpWorldPartitionHandlers
{
bool HandleCleanupInvalidDataLayers(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition)
{
#if MCP_HAS_DATALAYER_EDITOR
    UDataLayerEditorSubsystem* DataLayerSubsystem =
        GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
    if (!DataLayerSubsystem)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        return true;
    }

    TArray<UDataLayerInstance*> InvalidInstances;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    UDataLayerManager* DataLayerManager =
        WorldPartition ? WorldPartition->GetDataLayerManager() : nullptr;
    if (!DataLayerManager)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
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
    UDataLayerSubsystem* DataLayerSubsys =
        World ? World->GetSubsystem<UDataLayerSubsystem>() : nullptr;
    if (!DataLayerSubsys)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
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

    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Cleaned up %d invalid Data Layer Instances."), DeletedCount));
#else
    Bridge->SendAutomationError(RequestingSocket, RequestId,
        TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}
}
#endif
