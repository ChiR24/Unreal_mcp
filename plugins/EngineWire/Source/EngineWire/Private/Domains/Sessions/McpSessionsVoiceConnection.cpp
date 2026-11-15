#include "Domains/Sessions/McpSessionsPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool HandleEnableVoiceChat(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload->HasField(TEXT("voiceEnabled")))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("voiceEnabled is required (true to enable, false to disable)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const bool bEnabled = GetJsonBoolField(Payload, TEXT("voiceEnabled"), true);
    // The plugin does not link the VoiceChat module (an IVoiceChat implementation such as EOS or Vivox voice lives in its
    // own plugin), so voice chat is neither started nor stopped here; configure_voice_settings writes the VoIP config.
    const FString StatusMessage = TEXT("Voice chat module not available in this build");

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("voiceEnabled"), bEnabled);
    ResponseJson->SetBoolField(TEXT("success"), false);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("voiceChatAvailable"), false);

    FString Message = FString::Printf(TEXT("Voice chat %s: %s"),
        bEnabled ? TEXT("enabled") : TEXT("disabled"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, false, Message, ResponseJson, TEXT("NOT_SUPPORTED"));
    return true;
}
