#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleInspectGlobalAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    if (HandleInspectSettingsAction(Bridge, RequestId, SubAction, LowerSubAction,
                                    Payload, RequestingSocket, Resp) ||
        HandleInspectRuntimeReportAction(Bridge, RequestId, LowerSubAction,
                                         Payload, RequestingSocket) ||
        HandleInspectSearchAction(Bridge, RequestId, SubAction, LowerSubAction,
                                  Payload, RequestingSocket, Resp))
    {
        return true;
    }

    Bridge.SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Unsupported inspect action: %s"), *SubAction),
        TEXT("UNKNOWN_ACTION"));
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
