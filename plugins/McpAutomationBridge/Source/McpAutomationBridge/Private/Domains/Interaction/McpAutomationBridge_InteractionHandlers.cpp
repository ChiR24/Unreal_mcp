#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleManageInteractionAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_interaction"))
    {
        return false;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    using namespace McpInteractionHandlers;
    if (HandleInteractionComponentAuthoringAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleInteractionWidgetEventAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleInteractableInterfaceAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleDoorAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleSwitchAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleChestAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleLeverAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleDestructionAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleTriggerAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleInteractionInfoAction(this, RequestId, SubAction, Payload, RequestingSocket))
    {
        return true;
    }

    return false;
}
