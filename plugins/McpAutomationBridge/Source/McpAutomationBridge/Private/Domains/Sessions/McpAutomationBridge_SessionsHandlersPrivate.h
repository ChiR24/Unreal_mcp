#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if __has_include("VoiceChat.h")
#include "Features/IModularFeatures.h"
#include "VoiceChat.h"
#define MCP_HAS_VOICECHAT 1
#else
#define MCP_HAS_VOICECHAT 0
#endif

#if __has_include("OnlineSubsystem.h")
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystem.h"
#define MCP_HAS_ONLINE_SUBSYSTEM 1
#else
#define MCP_HAS_ONLINE_SUBSYSTEM 0
#endif

class FMcpBridgeWebSocket;
class UGameInstance;
class ULocalPlayer;
class UMcpAutomationBridgeSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpSessionsHandlers, Log, All);

namespace SessionsHelpers
{
UGameInstance* GetGameInstance();
ULocalPlayer* GetLocalPlayerByIndex(int32 PlayerIndex);
int32 GetLocalPlayerCount();
}

bool HandleConfigureSplitScreen(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleHostLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleEnableVoiceChat(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleMutePlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetSessionsInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
