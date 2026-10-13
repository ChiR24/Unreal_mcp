#include "Domains/Interaction/McpInteractionPrivate.h"

namespace McpInteractionHandlers
{
bool HandleLeverAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("create_lever_actor"))
    {
        return false;
    }
    UBlueprint* LeverBP = CreateInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT("/Game/Interactables"), TEXT("lever"), {
        {USceneComponent::StaticClass(), TEXT("Root"), nullptr, 0.0f},
        {UStaticMeshComponent::StaticClass(), TEXT("LeverBase"), nullptr, 0.0f},
        {USceneComponent::StaticClass(), TEXT("LeverPivot"), nullptr, 0.0f},
        {UStaticMeshComponent::StaticClass(), TEXT("LeverHandle"), TEXT("LeverPivot"), 0.0f},
        {USphereComponent::StaticClass(), TEXT("InteractionTrigger"), nullptr, 100.0f}});
    if (!LeverBP)
    {
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // Each press flips it, fires OnSwitchChanged and tilts the handle; it was components only.
    if (!AuthorSwitchLogic(Subsystem, RequestId, RequestingSocket, LeverBP, TEXT("lever"), /*bTilt=*/true, Result))
    {
        return true;
    }
    Result->SetStringField(TEXT("leverPath"), LeverBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), LeverBP->GetPathName());
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, LeverBP, Result,
                           {TEXT("created lever blueprint")}, TEXT("Lever actor created"));
    return true;
}
}
