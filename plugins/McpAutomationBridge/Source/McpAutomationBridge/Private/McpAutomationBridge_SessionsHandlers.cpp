#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

DEFINE_LOG_CATEGORY(LogMcpSessionsHandlers);

bool UMcpAutomationBridgeSubsystem::HandleManageSessionsAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction;
    if (Payload.IsValid() && Payload->HasField(TEXT("action")))
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    UE_LOG(LogMcpSessionsHandlers, Log, TEXT("HandleManageSessionsAction: SubAction=%s, RequestId=%s"), *SubAction, *RequestId);

    if (SubAction == TEXT("configure_local_session_settings"))
    {
        return HandleConfigureLocalSessionSettings(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_session_interface"))
    {
        return HandleConfigureSessionInterface(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_split_screen"))
    {
        return HandleConfigureSplitScreen(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_split_screen_type"))
    {
        return HandleSetSplitScreenType(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("add_local_player"))
    {
        return HandleAddLocalPlayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("remove_local_player"))
    {
        return HandleRemoveLocalPlayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_lan_play"))
    {
        return HandleConfigureLanPlay(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("host_lan_server"))
    {
        return HandleHostLanServer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("join_lan_server"))
    {
        return HandleJoinLanServer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("enable_voice_chat"))
    {
        return HandleEnableVoiceChat(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_voice_settings"))
    {
        return HandleConfigureVoiceSettings(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_voice_channel"))
    {
        return HandleSetVoiceChannel(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("mute_player"))
    {
        return HandleMutePlayer(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("set_voice_attenuation"))
    {
        return HandleSetVoiceAttenuation(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("configure_push_to_talk"))
    {
        return HandleConfigurePushToTalk(this, RequestId, Payload, Socket);
    }
    if (SubAction == TEXT("get_sessions_info"))
    {
        return HandleGetSessionsInfo(this, RequestId, Payload, Socket);
    }

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown manage_sessions action: %s"), *SubAction), nullptr);
    return true;
#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;
#endif
}
