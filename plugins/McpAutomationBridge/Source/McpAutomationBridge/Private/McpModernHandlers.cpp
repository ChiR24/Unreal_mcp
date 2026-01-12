#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// StateTree includes
#if MCP_HAS_STATETREE
#include "Components/StateTreeComponent.h"
#include "StateTree.h"
#endif

// Mass includes
#if MCP_HAS_MASS
#include "MassEntitySubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonTypes.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBindStateTree(const FString &RequestId, const FString &Action,
                                                        const TSharedPtr<FJsonObject> &Payload,
                                                        TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_STATETREE
    FString Target;
    if (!Payload->TryGetStringField(TEXT("target"), Target))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'target' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    AActor* Actor = FindActorByLabelOrName(Target);
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *Target), TEXT("ACTOR_NOT_FOUND"));
        return false;
    }

    UStateTree* StateTreeAsset = LoadObject<UStateTree>(nullptr, *AssetPath);
    if (!StateTreeAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("StateTree asset not found: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    UStateTreeComponent* StateTreeComp = Actor->FindComponentByClass<UStateTreeComponent>();
    if (!StateTreeComp)
    {
        StateTreeComp = NewObject<UStateTreeComponent>(Actor);
        StateTreeComp->RegisterComponent();
        Actor->AddInstanceComponent(StateTreeComp);
    }

    // Bind the asset
    StateTreeComp->SetStateTree(StateTreeAsset);
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("StateTree bound successfully"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("StateTree module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSpawnMassEntity(const FString &RequestId, const FString &Action,
                                                          const TSharedPtr<FJsonObject> &Payload,
                                                          TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if MCP_HAS_MASS
    FString ConfigPath;
    if (!Payload->TryGetStringField(TEXT("configPath"), ConfigPath))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'configPath' field"), TEXT("INVALID_PARAMS"));
        return false;
    }

    UWorld* World = GetActiveWorld();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No active world"), TEXT("NO_WORLD"));
        return false;
    }

    UMassEntityConfigAsset* ConfigAsset = LoadObject<UMassEntityConfigAsset>(nullptr, *ConfigPath);
    if (!ConfigAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("MassEntityConfig asset not found: %s"), *ConfigPath), TEXT("ASSET_NOT_FOUND"));
        return false;
    }

    UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
    if (!SpawnerSubsystem)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("MassSpawnerSubsystem not found"), TEXT("SUBSYSTEM_NOT_FOUND"));
        return false;
    }

    int32 Count = 1;
    if (Payload->HasField(TEXT("count")))
    {
        Count = Payload->GetIntegerField(TEXT("count"));
    }

    // SpawnEntities API requires FMassEntityConfig and Count
    FMassEntityConfig EntityConfig = ConfigAsset->GetConfig();
    
    // Note: FMassEntitySpawnDataGenerator and FMassSpawnedEntitiesCallback are required arguments in some versions
    // We'll use default constructed ones.
    SpawnerSubsystem->SpawnEntities(EntityConfig, Count, FMassEntitySpawnDataGenerator(), FMassSpawnedEntitiesCallback());

    SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Spawned %d Mass entities"), Count));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Mass module not enabled"), TEXT("MODULE_NOT_FOUND"));
    return false;
#endif
}