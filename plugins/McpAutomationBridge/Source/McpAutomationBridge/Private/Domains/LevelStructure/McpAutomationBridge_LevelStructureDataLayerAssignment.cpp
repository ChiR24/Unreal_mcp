#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "DataLayer/DataLayerEditorSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#endif
#include "WorldPartition/WorldPartition.h"

namespace McpLevelStructure
{

bool HandleAssignActorToDataLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    // actorName (label or name) or actorPath (the actor's object path); FindActorByNameInWorldForMcp takes either.
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("actorPath")});
    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName or actorPath is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (DataLayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("dataLayerName is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UWorld* World = nullptr;
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem =
        RequireDataLayerWorld(Subsystem, RequestId, Socket, TEXT("Actor-to-DataLayer assignment"), World);
    if (!DataLayerEditorSubsystem)
    {
        return true;
    }
    AActor* FoundActor = FindActorByNameInWorldForMcp(World, ActorName, true);

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
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, FoundActor->GetLevel(), Message, ResponseJson);
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
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, FoundActor->GetLevel(), Message, ResponseJson);
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
