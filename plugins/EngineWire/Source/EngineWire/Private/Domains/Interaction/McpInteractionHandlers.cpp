#include "Domains/Interaction/McpInteractionPrivate.h"

bool UEngineWireSubsystem::HandleManageInteractionAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

    using namespace McpInteractionHandlers;
    return HandleInteractionComponentAuthoringAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleInteractableInterfaceAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleDoorAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleSwitchAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleChestAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleLeverAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleTriggerAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
           HandleInteractionInfoAction(this, RequestId, SubAction, Payload, RequestingSocket);
}
