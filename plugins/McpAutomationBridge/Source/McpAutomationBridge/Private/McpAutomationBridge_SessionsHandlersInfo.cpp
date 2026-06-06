#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Dom/JsonValue.h"
#include "Editor.h"

bool HandleGetSessionsInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    TSharedPtr<FJsonObject> SessionsInfo = McpHandlerUtils::CreateResultObject();

    int32 LocalPlayerCount = SessionsHelpers::GetLocalPlayerCount();
    SessionsInfo->SetNumberField(TEXT("localPlayerCount"), LocalPlayerCount);

    bool bInPIE = GEditor && GEditor->PlayWorld != nullptr;
    SessionsInfo->SetBoolField(TEXT("inPlaySession"), bInPIE);
    SessionsInfo->SetStringField(TEXT("currentSessionName"), TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("isLANMatch"), false);
    SessionsInfo->SetNumberField(TEXT("maxPlayers"), 0);
    SessionsInfo->SetNumberField(TEXT("currentPlayers"), LocalPlayerCount);
    SessionsInfo->SetBoolField(TEXT("splitScreenEnabled"), LocalPlayerCount > 1);
    SessionsInfo->SetStringField(TEXT("splitScreenType"), LocalPlayerCount > 1 ? TEXT("Active") : TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("voiceChatEnabled"), false);
    SessionsInfo->SetBoolField(TEXT("isHosting"), false);
    SessionsInfo->SetStringField(TEXT("connectedServerAddress"), TEXT(""));

    TArray<TSharedPtr<FJsonValue>> VoiceChannels;
    SessionsInfo->SetArrayField(TEXT("activeVoiceChannels"), VoiceChannels);
    ResponseJson->SetObjectField(TEXT("sessionsInfo"), SessionsInfo);

    FString Message = FString::Printf(TEXT("Sessions info retrieved. Local players: %d, In PIE: %s"),
        LocalPlayerCount, bInPIE ? TEXT("Yes") : TEXT("No"));
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
#endif
