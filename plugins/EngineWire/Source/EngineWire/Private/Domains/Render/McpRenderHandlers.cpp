#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"

#include "Domains/Render/McpRenderPrivate.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"

bool UEngineWireSubsystem::HandleRenderAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // build_environment's route calls this with manage_render and a payload whose sub-action it claimed.
    const FString SubAction = McpConsolidatedActions::GetPayloadSubAction(Payload);

    using namespace McpRenderHandlers;
    if (HandleRenderConsoleAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderLightingAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderReflectionResolutionAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderReflectionAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderSceneCaptureAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderPostProcessColorAction(this, RequestId, SubAction, Payload, RequestingSocket) ||
        HandleRenderPostProcessLensAction(this, RequestId, SubAction, Payload, RequestingSocket))
    {
        return true;
    }

    SendAutomationError(
        RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
