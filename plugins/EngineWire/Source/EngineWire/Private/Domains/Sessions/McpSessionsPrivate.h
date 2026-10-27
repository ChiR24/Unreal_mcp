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
class UEngineWireSubsystem;

DECLARE_LOG_CATEGORY_EXTERN(LogMcpSessionsHandlers, Log, All);

namespace SessionsHelpers
{
UGameInstance* GetGameInstance();
ULocalPlayer* GetLocalPlayerByIndex(int32 PlayerIndex);
int32 GetLocalPlayerCount();
}

bool HandleConfigureSplitScreen(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleAddLocalPlayer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleRemoveLocalPlayer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleHostLanServer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleEnableVoiceChat(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleMutePlayer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleGetSessionsInfo(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureLanPlay(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleJoinLanServer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureSessionInterface(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
bool HandleConfigureVoiceSettings(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket);
