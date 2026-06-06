#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"

#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#endif
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleAssignActorToDataLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (DataLayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("dataLayerName is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Check if World Partition is enabled
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level. Data layers require World Partition."), nullptr, TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }

    // CRITICAL: Check if the level uses External Objects (One File Per Actor / OFPA)
    // Actor-to-DataLayer assignment requires OFPA for actors to be compatible with data layers.
    // Non-OFPA actors cannot be assigned to data layers.
    ULevel* PersistentLevel = World->PersistentLevel;
    if (!PersistentLevel || !PersistentLevel->IsUsingExternalObjects())
    {
        TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
        ErrorDetails->SetStringField(TEXT("reason"), TEXT("One File Per Actor (OFPA) / External Actors is not enabled for this level."));
        ErrorDetails->SetStringField(TEXT("solution"), TEXT("Enable 'Use External Actors' in World Partition settings. Actors must be external to be compatible with data layers."));
        ErrorDetails->SetBoolField(TEXT("worldPartitionEnabled"), true);
        ErrorDetails->SetBoolField(TEXT("externalActorsEnabled"), PersistentLevel ? PersistentLevel->IsUsingExternalObjects() : false);

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Actor-to-DataLayer assignment requires 'One File Per Actor' (External Actors). Actors must be stored as external packages to be compatible with data layers."),
            ErrorDetails, TEXT("EXTERNAL_ACTORS_NOT_ENABLED"));
        return true;
    }

    // Get the Data Layer Editor Subsystem
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
    if (!DataLayerEditorSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data Layer Editor Subsystem not available"), nullptr, TEXT("SUBSYSTEM_NOT_AVAILABLE"));
        return true;
    }

    // Find the actor
    AActor* FoundActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            FoundActor = *It;
            break;
        }
    }

    if (!FoundActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // Find the data layer instance by name
    // Try multiple lookup methods to handle both short name and full name matching
    UDataLayerInstance* DataLayerInstance = nullptr;

    // Method 1: Direct FName lookup (for full names)
    DataLayerInstance = DataLayerEditorSubsystem->GetDataLayerInstance(FName(*DataLayerName));

    // Method 2: If not found, search by short name (case-insensitive)
    if (!DataLayerInstance)
    {
        TArray<UDataLayerInstance*> AllDataLayers = DataLayerEditorSubsystem->GetAllDataLayers();
        for (UDataLayerInstance* DL : AllDataLayers)
        {
            if (DL)
            {
                // Compare by short name (case-insensitive for robustness)
                FString ShortName = DL->GetDataLayerShortName();
                if (ShortName.Equals(DataLayerName, ESearchCase::IgnoreCase))
                {
                    DataLayerInstance = DL;
                    break;
                }
                // Also try full name
                FString FullName = DL->GetDataLayerFullName();
                if (FullName.Equals(DataLayerName, ESearchCase::IgnoreCase))
                {
                    DataLayerInstance = DL;
                    break;
                }
            }
        }
    }

    if (!DataLayerInstance)
    {
        // Build a list of available data layers for the error message
        TArray<UDataLayerInstance*> AllDataLayers = DataLayerEditorSubsystem->GetAllDataLayers();
        TArray<FString> AvailableNames;
        for (UDataLayerInstance* DL : AllDataLayers)
        {
            if (DL)
            {
                AvailableNames.Add(DL->GetDataLayerShortName());
            }
        }

        FString AvailableStr = AvailableNames.Num() > 0
            ? FString::Join(AvailableNames, TEXT(", "))
            : TEXT("(none)");

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Data layer not found: '%s'. Available data layers: %s"), *DataLayerName, *AvailableStr), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    // IDEMPOTENCY: Check if actor is already in the target data layer before attempting to add
    // This makes the operation idempotent - returns success whether actor is newly added or already present
    bool bAlreadyInLayer = FoundActor->ContainsDataLayer(DataLayerInstance);

    if (bAlreadyInLayer)
    {
        // Already assigned - return success (idempotent behavior)
        TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(ResponseJson, FoundActor);
        ResponseJson->SetStringField(TEXT("actorName"), ActorName);
        ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
        ResponseJson->SetBoolField(TEXT("assigned"), true);
        ResponseJson->SetBoolField(TEXT("alreadyAssigned"), true);

        FString Message = FString::Printf(TEXT("Actor '%s' is already in data layer '%s'"),
            *ActorName, *DataLayerName);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
        return true;
    }

    // Use the real API to add the actor to the data layer
    bool bSuccess = DataLayerEditorSubsystem->AddActorToDataLayer(FoundActor, DataLayerInstance);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, FoundActor);
    ResponseJson->SetStringField(TEXT("actorName"), ActorName);
    ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
    ResponseJson->SetBoolField(TEXT("assigned"), bSuccess);

    if (bSuccess)
    {
        FString Message = FString::Printf(TEXT("Assigned actor '%s' to data layer '%s'"),
            *ActorName, *DataLayerName);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    }
    else
    {
        // This should rarely happen now - only if actor is incompatible with data layers
        ResponseJson->SetStringField(TEXT("reason"), TEXT("Actor is not compatible with data layers"));
        FString Message = FString::Printf(TEXT("Failed to assign actor '%s' to data layer '%s'. Actor may not be compatible with data layers."),
            *ActorName, *DataLayerName);
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Message, ResponseJson);
    }
#else
    // UE 5.0 does not support the new DataLayer API
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data layer assignment requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

}
#endif
