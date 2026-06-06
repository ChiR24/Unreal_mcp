#include "McpAutomationBridge_InteractionHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleCreateInteractionComponent(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Actor = nullptr;
    if (!McpInteractionHandlers::FindEditorActorByName(ActorName, Actor))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"), TEXT("NO_WORLD"));
        return true;
    }
    if (!Actor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found"), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USceneComponent* InteractionComp = NewObject<USceneComponent>(Actor, FName(TEXT("InteractionComponent")));
    if (!InteractionComp)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interaction component"), TEXT("CREATE_FAILED"));
        return true;
    }
    InteractionComp->RegisterComponent();
    Actor->AddInstanceComponent(InteractionComp);

    double InteractionDistance = 200.0;
    Payload->TryGetNumberField(TEXT("interactionDistance"), InteractionDistance);
    bool RequiresLineOfSight = true;
    Payload->TryGetBoolField(TEXT("requiresLineOfSight"), RequiresLineOfSight);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("componentName"), InteractionComp->GetName());
    Resp->SetNumberField(TEXT("interactionDistance"), InteractionDistance);
    Resp->SetBoolField(TEXT("requiresLineOfSight"), RequiresLineOfSight);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction component created"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleConfigureInteractionTrace(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double TraceDistance = 500.0;
    Payload->TryGetNumberField(TEXT("traceDistance"), TraceDistance);
    FString TraceChannel = TEXT("Visibility");
    Payload->TryGetStringField(TEXT("traceChannel"), TraceChannel);
    bool UseComplexCollision = false;
    Payload->TryGetBoolField(TEXT("useComplexCollision"), UseComplexCollision);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetNumberField(TEXT("traceDistance"), TraceDistance);
    Resp->SetStringField(TEXT("traceChannel"), TraceChannel);
    Resp->SetBoolField(TEXT("useComplexCollision"), UseComplexCollision);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction trace configured"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleConfigureInteractionWidget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString ActorName;
    if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) || ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("actorName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString WidgetClass;
    Payload->TryGetStringField(TEXT("widgetClass"), WidgetClass);
    FString WidgetText;
    if (!Payload->TryGetStringField(TEXT("widgetText"), WidgetText))
    {
        WidgetText = TEXT("Interact");
    }
    bool ShowOnHover = true;
    Payload->TryGetBoolField(TEXT("showOnHover"), ShowOnHover);
    double OffsetZ = 100.0;
    Payload->TryGetNumberField(TEXT("offsetZ"), OffsetZ);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), ActorName);
    Resp->SetStringField(TEXT("widgetClass"), WidgetClass);
    Resp->SetStringField(TEXT("widgetText"), WidgetText);
    Resp->SetBoolField(TEXT("showOnHover"), ShowOnHover);
    Resp->SetNumberField(TEXT("offsetZ"), OffsetZ);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction widget configured"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}
