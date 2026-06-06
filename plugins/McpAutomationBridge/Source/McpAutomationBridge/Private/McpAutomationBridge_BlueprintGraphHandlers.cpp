#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_blueprint"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            TEXT("Missing payload for blueprint graph action."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    McpBlueprintGraphHandlers::FActionContext Context{
        this,
        RequestId,
        Payload,
        RequestingSocket,
        GetJsonStringField(Payload, TEXT("subAction"))};

    if (!McpBlueprintGraphHandlers::ValidateProvidedPaths(Context))
    {
        return true;
    }

    if (McpBlueprintGraphHandlers::HandleListNodeTypes(Context))
    {
        return true;
    }

    if (!McpBlueprintGraphHandlers::PrepareBlueprintAndGraph(Context))
    {
        return true;
    }

    if (McpBlueprintGraphHandlers::HandleNodeCreationAction(Context) ||
        McpBlueprintGraphHandlers::HandlePinMutationAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeMutationAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeQueryAction(Context) ||
        McpBlueprintGraphHandlers::HandleNodeDetailAction(Context))
    {
        return true;
    }

    Context.SendError(
        FString::Printf(TEXT("Unknown subAction: %s"), *Context.SubAction),
        TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(
        RequestingSocket,
        RequestId,
        TEXT("Blueprint graph actions are editor-only."),
        TEXT("EDITOR_ONLY"));
    return true;
#endif
}
