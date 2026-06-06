#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

#if WITH_EDITOR
namespace McpWorldPartitionHandlers
{
bool HandleCreateDataLayer(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition)
{
#if MCP_HAS_DATALAYER_EDITOR
    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));
    if (DataLayerName.IsEmpty())
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing dataLayerName."), TEXT("INVALID_PARAMS"));
        return true;
    }

    UDataLayerEditorSubsystem* DataLayerSubsystem =
        GEditor->GetEditorSubsystem<UDataLayerEditorSubsystem>();
    if (!DataLayerSubsystem)
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("DataLayerEditorSubsystem not found."), TEXT("SUBSYSTEM_NOT_FOUND"));
        return true;
    }

    bool bExists = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
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
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("DataLayer '%s' already exists."), *DataLayerName));
        return true;
    }

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
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("DataLayer '%s' created."), *DataLayerName));
    }
    else
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Failed to create DataLayer (Subsystem returned null)."),
            TEXT("CREATE_FAILED"));
    }
#else
    Bridge->SendAutomationError(RequestingSocket, RequestId,
        TEXT("DataLayerEditorSubsystem not available."), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}
}
#endif
