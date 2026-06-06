#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace
{
#if WITH_EDITOR
bool AddBlueprintInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Result,
    const FString& Path,
    const FString& TypeField,
    const FString& PathField)
{
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(Path, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return false;
    }

    Result->SetStringField(TEXT("assetType"), TypeField);
    Result->SetStringField(PathField, Path);
    if (PathField == TEXT("blueprintPath"))
    {
        Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
    }
    return true;
}
#endif
}

namespace McpInteractionHandlers
{
bool HandleInteractionInfoAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("get_interaction_info"))
    {
        return false;
    }

    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    const FString DoorPath = GetJsonStringField(Payload, TEXT("doorPath"));
    const FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
    const FString ChestPath = GetJsonStringField(Payload, TEXT("chestPath"));
    const FString TriggerPath = GetJsonStringField(Payload, TEXT("triggerPath"));
    if (BlueprintPath.IsEmpty() && ActorName.IsEmpty() && DoorPath.IsEmpty() &&
        SwitchPath.IsEmpty() && ChestPath.IsEmpty() && TriggerPath.IsEmpty())
    {
        Subsystem->SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("At least one path parameter is required (blueprintPath, actorName, doorPath, switchPath, chestPath, or triggerPath)"),
            TEXT("MISSING_PARAMETER"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
#if WITH_EDITOR
    if (!BlueprintPath.IsEmpty())
    {
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, BlueprintPath, TEXT("Blueprint"), TEXT("blueprintPath")))
        {
            return true;
        }
    }
    else if (!ActorName.IsEmpty())
    {
        AActor* FoundActor = nullptr;
        if (!FindEditorActorByName(ActorName, FoundActor))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
            return true;
        }
        if (!FoundActor)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
            return true;
        }
        Result->SetStringField(TEXT("assetType"), TEXT("Actor"));
        Result->SetStringField(TEXT("actorName"), FoundActor->GetName());
        Result->SetStringField(TEXT("actorClass"), FoundActor->GetClass()->GetName());
    }
    else if (!DoorPath.IsEmpty())
    {
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, DoorPath, TEXT("Door"), TEXT("doorPath")))
        {
            return true;
        }
    }
    else if (!SwitchPath.IsEmpty())
    {
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, SwitchPath, TEXT("Switch"), TEXT("switchPath")))
        {
            return true;
        }
    }
    else if (!ChestPath.IsEmpty())
    {
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, ChestPath, TEXT("Chest"), TEXT("chestPath")))
        {
            return true;
        }
    }
    else if (!TriggerPath.IsEmpty())
    {
        if (!AddBlueprintInfo(Subsystem, RequestId, RequestingSocket, Result, TriggerPath, TEXT("Trigger"), TEXT("triggerPath")))
        {
            return true;
        }
    }
#endif
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction info retrieved"), Result);
    return true;
}
}
