#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Render/McpAutomationBridge_RenderHandlersPrivate.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

bool UMcpAutomationBridgeSubsystem::HandleRenderAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Action.Equals(TEXT("manage_render"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(
            RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }
    SubAction = SubAction.ToLower();
    SubAction.ReplaceInline(TEXT("-"), TEXT("_"));
    SubAction.ReplaceInline(TEXT(" "), TEXT("_"));

    using namespace McpRenderHandlers;
    if (SubAction == TEXT("create_render_target"))
    {
        return HandleCreateRenderTarget(this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("nanite_rebuild_mesh"))
    {
        // One implementation for both routes: manage_asset reaches the same handler.
        return HandleNaniteRebuildMesh(RequestId, SubAction, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("lumen_update_scene"))
    {
        return HandleLumenUpdateScene(this, RequestId, RequestingSocket);
    }
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
