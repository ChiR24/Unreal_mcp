#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#if __has_include("VoiceChat.h")
#include "VoiceChat.h"
#define MCP_HAS_VOICECHAT 1
#else
#define MCP_HAS_VOICECHAT 0
#endif

bool HandleEnableVoiceChat(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("voiceEnabled")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("voiceEnabled is required (true to enable, false to disable)"), nullptr);
        return true;
    }

    bool bEnabled = GetJsonBoolField(Payload, TEXT("voiceEnabled"), true);
    bool bSuccess = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (VoiceChat)
    {
        if (bEnabled)
        {
            if (!VoiceChat->IsInitialized())
            {
                bSuccess = VoiceChat->Initialize();
                if (bSuccess)
                {
                    StatusMessage = TEXT("Voice chat initialized");
                    VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda(
                        [](const FVoiceChatResult& Result)
                        {
                            UE_LOG(LogMcpSessionsHandlers, Log, TEXT("VoiceChat Connect Result: %s"),
                                Result.IsSuccess() ? TEXT("Success") : *Result.ErrorDesc);
                        }));
                }
                else
                {
                    StatusMessage = TEXT("Failed to initialize voice chat");
                }
            }
            else
            {
                bSuccess = true;
                StatusMessage = TEXT("Voice chat already initialized");
            }
        }
        else
        {
            if (VoiceChat->IsConnected())
            {
                VoiceChat->Disconnect(FOnVoiceChatDisconnectCompleteDelegate::CreateLambda(
                    [VoiceChat](const FVoiceChatResult& Result)
                    {
                        if (VoiceChat->IsInitialized())
                        {
                            VoiceChat->Uninitialize();
                        }
                    }));
                bSuccess = true;
                StatusMessage = TEXT("Voice chat disconnecting");
            }
            else if (VoiceChat->IsInitialized())
            {
                bSuccess = VoiceChat->Uninitialize();
                StatusMessage = bSuccess ? TEXT("Voice chat uninitialized") : TEXT("Failed to uninitialize voice chat");
            }
            else
            {
                bSuccess = true;
                StatusMessage = TEXT("Voice chat already disabled");
            }
        }
    }
    else
    {
        StatusMessage = TEXT("IVoiceChat interface not available - no voice chat plugin loaded");
        bSuccess = false;
    }
#else
    StatusMessage = TEXT("Voice chat module not available in this build");
    bSuccess = true;
#endif

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("voiceEnabled"), bEnabled);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
#if MCP_HAS_VOICECHAT
    ResponseJson->SetBoolField(TEXT("voiceChatAvailable"), IVoiceChat::Get() != nullptr);
#else
    ResponseJson->SetBoolField(TEXT("voiceChatAvailable"), false);
#endif

    FString Message = FString::Printf(TEXT("Voice chat %s: %s"),
        bEnabled ? TEXT("enabled") : TEXT("disabled"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

#undef MCP_HAS_VOICECHAT
#endif
