#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

bool UMcpAutomationBridgeSubsystem::HandleManageGameFrameworkAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_game_framework"))
    {
        return false;
    }

#if !WITH_EDITOR
    SendAutomationError(RequestingSocket, RequestId, TEXT("Game framework handlers require editor build."), TEXT("EDITOR_ONLY"));
    return true;
#else
    McpGameFrameworkHandlers::FActionContext Context =
        McpGameFrameworkHandlers::MakeActionContext(this, RequestId, Payload, RequestingSocket);

    if (!McpGameFrameworkHandlers::ValidateCommonFields(Context))
    {
        return true;
    }

    if (McpGameFrameworkHandlers::HandleCoreClassAction(Context)) return true;
    if (McpGameFrameworkHandlers::HandleGameModeConfigAction(Context)) return true;
    if (McpGameFrameworkHandlers::HandleMatchFlowAction(Context)) return true;
    if (McpGameFrameworkHandlers::HandlePlayerFlowAction(Context)) return true;
    if (McpGameFrameworkHandlers::HandleInfoAction(Context)) return true;

    Context.SendError(
        FString::Printf(TEXT("Unknown subAction: %s"), *Context.SubAction),
        TEXT("UNKNOWN_SUBACTION"));
    return true;
#endif
}
