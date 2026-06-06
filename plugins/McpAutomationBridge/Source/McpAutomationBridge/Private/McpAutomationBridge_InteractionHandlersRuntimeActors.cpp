#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace
{
#if WITH_EDITOR
FVector GetOptionalLocation(const TSharedPtr<FJsonObject>& Payload)
{
    FVector Location = FVector::ZeroVector;
    const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3)
    {
        Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());
    }
    return Location;
}

AActor* SpawnBasicActor(UWorld* World, const FString& Name, const FVector& Location)
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*Name);
    return World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
}

void AddRuntimeInteractionComponents(AActor* Actor)
{
    UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
    Mesh->RegisterComponent();
    Actor->SetRootComponent(Mesh);
    USceneComponent* InteractionComp = NewObject<USceneComponent>(Actor, FName(TEXT("InteractionComponent")));
    InteractionComp->RegisterComponent();
}
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateDoorActor(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString DoorName;
    if (!Payload->TryGetStringField(TEXT("doorName"), DoorName) || DoorName.IsEmpty())
    {
        DoorName = TEXT("BP_Door");
    }
    FString DoorType = TEXT("swing");
    Payload->TryGetStringField(TEXT("doorType"), DoorType);
    bool IsLocked = false;
    Payload->TryGetBoolField(TEXT("isLocked"), IsLocked);
    FString RequiredKey;
    Payload->TryGetStringField(TEXT("requiredKey"), RequiredKey);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"), TEXT("NO_WORLD"));
        return true;
    }
    AActor* DoorActor = SpawnBasicActor(World, DoorName, GetOptionalLocation(Payload));
    if (!DoorActor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn door actor"), TEXT("SPAWN_FAILED"));
        return true;
    }
    AddRuntimeInteractionComponents(DoorActor);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("doorName"), DoorActor->GetName());
    Resp->SetStringField(TEXT("doorType"), DoorType);
    Resp->SetBoolField(TEXT("isLocked"), IsLocked);
    Resp->SetStringField(TEXT("requiredKey"), RequiredKey);
    Resp->SetStringField(TEXT("actorPath"), DoorActor->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door actor created"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateSwitchActor(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SwitchName;
    if (!Payload->TryGetStringField(TEXT("switchName"), SwitchName) || SwitchName.IsEmpty())
    {
        SwitchName = TEXT("BP_Switch");
    }
    FString SwitchType = TEXT("lever");
    Payload->TryGetStringField(TEXT("switchType"), SwitchType);
    bool IsToggle = true;
    Payload->TryGetBoolField(TEXT("isToggle"), IsToggle);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"), TEXT("NO_WORLD"));
        return true;
    }
    AActor* SwitchActor = SpawnBasicActor(World, SwitchName, GetOptionalLocation(Payload));
    if (!SwitchActor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn switch actor"), TEXT("SPAWN_FAILED"));
        return true;
    }
    AddRuntimeInteractionComponents(SwitchActor);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("switchName"), SwitchActor->GetName());
    Resp->SetStringField(TEXT("switchType"), SwitchType);
    Resp->SetBoolField(TEXT("isToggle"), IsToggle);
    Resp->SetStringField(TEXT("actorPath"), SwitchActor->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch actor created"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateChestActor(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString ChestName;
    if (!Payload->TryGetStringField(TEXT("chestName"), ChestName) || ChestName.IsEmpty())
    {
        ChestName = TEXT("BP_Chest");
    }
    bool IsLocked = false;
    Payload->TryGetBoolField(TEXT("isLocked"), IsLocked);
    FString RequiredKey;
    Payload->TryGetStringField(TEXT("requiredKey"), RequiredKey);
    int32 MaxItems = 10;
    Payload->TryGetNumberField(TEXT("maxItems"), MaxItems);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world"), TEXT("NO_WORLD"));
        return true;
    }
    AActor* ChestActor = SpawnBasicActor(World, ChestName, GetOptionalLocation(Payload));
    if (!ChestActor)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn chest actor"), TEXT("SPAWN_FAILED"));
        return true;
    }
    AddRuntimeInteractionComponents(ChestActor);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("chestName"), ChestActor->GetName());
    Resp->SetBoolField(TEXT("isLocked"), IsLocked);
    Resp->SetStringField(TEXT("requiredKey"), RequiredKey);
    Resp->SetNumberField(TEXT("maxItems"), MaxItems);
    Resp->SetStringField(TEXT("actorPath"), ChestActor->GetPathName());
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest actor created"), Resp);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
#endif
    return true;
}
