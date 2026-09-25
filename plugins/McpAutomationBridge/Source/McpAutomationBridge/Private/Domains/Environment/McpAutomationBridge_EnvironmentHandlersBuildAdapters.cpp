#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

void MarkActorConfigurationResult(FEnvironmentBuildContext &Context, const bool bResult, const FString &Message, const FString &ErrorCode)
{
    Context.bSuccess = bResult;
    Context.Message = Message;
    Context.ErrorCode = bResult ? FString() : ErrorCode;
}

bool ConfigureEnvironmentActor(
    FEnvironmentBuildContext &Context,
    const FString &ActorClassPath,
    const FString &DefaultActorName,
    const FString &ComponentClassPath)
{
    FString Message;
    FString ErrorCode;
    bool bResult = McpConfigureActorAndComponent(
        Context.Payload,
        ActorClassPath,
        DefaultActorName,
        ComponentClassPath,
        Context.Resp,
        Message,
        ErrorCode);
    if (bResult && !ComponentClassPath.IsEmpty())
    {
        FString ComponentPath;
        if (!Context.Resp->TryGetStringField(TEXT("componentPath"), ComponentPath) ||
            ComponentPath.IsEmpty())
        {
            bResult = false;
            Message = FString::Printf(
                TEXT("Required component could not be configured: %s"), *ComponentClassPath);
            ErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        }
    }
    const TArray<TSharedPtr<FJsonValue>> *ConfigurationErrors = nullptr;
    if (bResult &&
        Context.Resp->TryGetArrayField(TEXT("configurationErrors"), ConfigurationErrors) &&
        ConfigurationErrors && !ConfigurationErrors->IsEmpty())
    {
        bResult = false;
        TArray<FString> Reasons;
        for (const TSharedPtr<FJsonValue> &Reason : *ConfigurationErrors)
        {
            Reasons.Add(Reason.IsValid() ? Reason->AsString() : FString());
        }
        Message = FString::Printf(TEXT("Could not apply: %s"), *FString::Join(Reasons, TEXT("; ")));
        ErrorCode = TEXT("CONFIGURATION_FAILED");
    }
    MarkActorConfigurationResult(Context, bResult, Message, ErrorCode);
    return true;
}

bool HandleBuildEnvironmentEditorAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSub, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    bool bSuccess = false;
    FString Message;
    FString ErrorCode;
    FEnvironmentBuildContext Context{Payload, Resp, bSuccess, Message, ErrorCode};

    const bool bHandled =
        HandleBuildSnapshotAndDeletionAction(LowerSub, Context) ||
        HandleBuildLandscapeAndFoliageAction(LowerSub, Context) ||
        HandleBuildSkyWeatherAction(LowerSub, Context) ||
        HandleBuildWaterAction(LowerSub, Context);

    if (!bHandled)
    {
        Bridge.SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Unknown build_environment action: %s"), *LowerSub),
            TEXT("UNKNOWN_ACTION"));
        return true;
    }

    Bridge.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        bSuccess,
        Message.IsEmpty() ? FString::Printf(TEXT("build_environment action completed: %s"), *LowerSub) : Message,
        Resp,
        ErrorCode);
    return true;
}

}
#endif
