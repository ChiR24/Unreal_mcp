#include "McpAutomationBridge_InteractionHandlersPrivate.h"

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

    if (SubAction == TEXT("create_interaction_component"))
    {
        return HandleCreateInteractionComponent(RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("configure_interaction_trace"))
    {
        return HandleConfigureInteractionTrace(RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("configure_interaction_widget"))
    {
        return HandleConfigureInteractionWidget(RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_door_actor"))
    {
        return HandleCreateDoorActor(RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_switch_actor"))
    {
        return HandleCreateSwitchActor(RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("create_chest_actor"))
    {
        return HandleCreateChestActor(RequestId, Payload, RequestingSocket);
    }

    return false;
}
