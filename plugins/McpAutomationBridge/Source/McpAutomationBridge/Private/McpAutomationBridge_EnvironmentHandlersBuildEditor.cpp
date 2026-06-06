#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleBuildEnvironmentEditorAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSub, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("action"), LowerSub);
    bool bSuccess = true;
    FString Message = FString::Printf(TEXT("Environment action '%s' completed"), *LowerSub);
    FString ErrorCode;

    FEnvironmentBuildContext Context{Payload, Resp, bSuccess, Message, ErrorCode};
    const bool bHandled =
        HandleBuildSnapshotAndDeletionAction(LowerSub, Context) ||
        HandleBuildLandscapeAndFoliageAction(LowerSub, Context) ||
        HandleBuildSkyWeatherAction(LowerSub, Context) ||
        HandleBuildWaterAction(LowerSub, Context);

    if (!bHandled)
    {
        bSuccess = false;
        Message = FString::Printf(TEXT("Environment action '%s' not implemented"), *LowerSub);
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
