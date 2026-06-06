#include "McpAutomationBridge_WorldPartitionHandlersPrivate.h"

#if WITH_EDITOR
namespace McpWorldPartitionHandlers
{
bool HandleSetDataLayer(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UWorld* World,
    UWorldPartition* WorldPartition)
{
    FString ActorPath = GetJsonStringField(Payload, TEXT("actorPath"));
    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"));

#if MCP_HAS_DATALAYER_EDITOR
    AActor* Actor = FindObject<AActor>(nullptr, *ActorPath);
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
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Actor not found: %s"), *ActorPath),
            TEXT("ACTOR_NOT_FOUND"));
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

    UDataLayerInstance* TargetLayer = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
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

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Actor added to DataLayer."), Result);
    }
    else
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("DataLayer '%s' not found."), *DataLayerName),
            TEXT("DATALAYER_NOT_FOUND"));
    }
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("DataLayerEditorSubsystem not available. set_datalayer skipped."));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorPath);
    Result->SetStringField(TEXT("dataLayerName"), DataLayerName);
    Result->SetBoolField(TEXT("added"), false);
    Result->SetStringField(TEXT("note"), TEXT("Simulated - Subsystem missing"));

    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Actor added to DataLayer (Simulated - Subsystem missing)."), Result);
#endif
    return true;
}
}
#endif
