#include "Domains/Inspect/McpInspectShared.h"

namespace McpInspectHandlers {

bool HandleInspectGlobalAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Every name the dispatcher sends here is answered by one of these three.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    return HandleInspectSettingsAction(Bridge, RequestId, SubAction, LowerSubAction, Payload, RequestingSocket, Resp) ||
           HandleInspectRuntimeReportAction(Bridge, RequestId, LowerSubAction, Payload, RequestingSocket) ||
           HandleInspectSearchAction(Bridge, RequestId, SubAction, LowerSubAction, Payload, RequestingSocket, Resp);
}

} // namespace McpInspectHandlers
