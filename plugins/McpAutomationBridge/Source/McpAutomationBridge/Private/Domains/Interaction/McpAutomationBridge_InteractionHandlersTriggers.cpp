#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleTriggerAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction != TEXT("create_trigger_actor"))
    {
        return false;
    }
    // The volume is the blueprint's root: a 200 sphere, a 50x100 capsule, else a 100 box.
    const FString TriggerShape = GetJsonStringField(Payload, TEXT("triggerShape"), TEXT("box"));
    const bool bSphere = TriggerShape == TEXT("sphere");
    const bool bCapsule = TriggerShape == TEXT("capsule");
    UClass* ShapeClass = bSphere    ? USphereComponent::StaticClass()
                         : bCapsule ? UCapsuleComponent::StaticClass()
                                    : UBoxComponent::StaticClass();
    UBlueprint* TriggerBP = CreateInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT("/Game/Triggers"), TEXT("trigger"), {
        {ShapeClass, TEXT("TriggerVolume"), nullptr, bSphere ? 200.0f : bCapsule ? 50.0f : 100.0f}});
    if (!TriggerBP)
    {
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("triggerPath"), TriggerBP->GetPathName());
    Result->SetStringField(TEXT("blueprintPath"), TriggerBP->GetPathName());
    Result->SetStringField(TEXT("triggerShape"), TriggerShape);
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, TriggerBP, Result,
                           {TEXT("created trigger blueprint")}, TEXT("Trigger actor created"));
    return true;
}
}
