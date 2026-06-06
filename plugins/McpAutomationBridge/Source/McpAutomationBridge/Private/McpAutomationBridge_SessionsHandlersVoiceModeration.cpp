#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Misc/EngineVersionComparison.h"

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

bool HandleMutePlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString PlayerName = GetJsonStringField(Payload, TEXT("playerName"), TEXT(""));
    FString TargetPlayerId = GetJsonStringField(Payload, TEXT("targetPlayerId"), TEXT(""));
    bool bMuted = GetJsonBoolField(Payload, TEXT("muted"), true);
    int32 LocalPlayerNum = static_cast<int32>(GetJsonNumberField(Payload, TEXT("localPlayerNum"), 0));
    bool bSystemWide = GetJsonBoolField(Payload, TEXT("systemWide"), false);

    if (PlayerName.IsEmpty() && TargetPlayerId.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Either playerName or targetPlayerId is required"), nullptr);
        return true;
    }

    FString TargetIdentifier = !TargetPlayerId.IsEmpty() ? TargetPlayerId : PlayerName;
    bool bSuccess = false;
    bool bAppliedToVoiceInterface = false;
    bool bAppliedToFallbackState = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    static FName VoiceChatFeatureName = FName(TEXT("VoiceChat"));
    if (IModularFeatures::Get().IsModularFeatureAvailable(VoiceChatFeatureName))
    {
        IVoiceChat* VoiceChat = &IModularFeatures::Get().GetModularFeature<IVoiceChat>(VoiceChatFeatureName);
        if (VoiceChat)
        {
            if (VoiceChat->IsInitialized() && VoiceChat->IsLoggedIn())
            {
                VoiceChat->SetPlayerMuted(TargetIdentifier, bMuted);
                bSuccess = true;
                bAppliedToVoiceInterface = true;
                StatusMessage = FString::Printf(TEXT("Player '%s' %s via IVoiceChat"),
                    *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"));
            }
            else if (VoiceChat->IsInitialized())
            {
                TArray<FString> PlayersToBlock;
                PlayersToBlock.Add(TargetIdentifier);
                if (bMuted)
                {
                    VoiceChat->BlockPlayers(PlayersToBlock);
                }
                else
                {
                    VoiceChat->UnblockPlayers(PlayersToBlock);
                }
                bSuccess = true;
                bAppliedToVoiceInterface = true;
                StatusMessage = FString::Printf(TEXT("Player '%s' %s locally (voice server not connected)"),
                    *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"));
            }
            else
            {
                bSuccess = false;
                StatusMessage = TEXT("VoiceChat module is available but not initialized; mute was not applied");
            }
        }
    }
    else
#endif
#if MCP_HAS_ONLINE_SUBSYSTEM
    {
        IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
        if (OnlineSub)
        {
            IOnlineVoicePtr VoiceInterface = OnlineSub->GetVoiceInterface();
            if (VoiceInterface.IsValid())
            {
                IOnlineIdentityPtr IdentityInterface = OnlineSub->GetIdentityInterface();
                if (IdentityInterface.IsValid())
                {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
                    FUniqueNetIdPtr NetId = IdentityInterface->CreateUniquePlayerId(TargetPlayerId);
                    if (NetId.IsValid())
                    {
                        if (bMuted)
                        {
                            bSuccess = VoiceInterface->MuteRemoteTalker(LocalPlayerNum, *NetId, bSystemWide);
                        }
                        else
                        {
                            bSuccess = VoiceInterface->UnmuteRemoteTalker(LocalPlayerNum, *NetId, bSystemWide);
                        }
                        StatusMessage = bSuccess
                            ? FString::Printf(TEXT("Player '%s' %s via OnlineSubsystem"),
                                *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"))
                            : TEXT("Voice interface mute operation failed");
                        bAppliedToVoiceInterface = bSuccess;
                    }
                    else
                    {
                        bSuccess = false;
                        StatusMessage = TEXT("Unable to resolve target player net ID; mute was not applied");
                    }
#else
                    bSuccess = false;
                    StatusMessage = TEXT("UE 5.7+ remote mute requires session-based player lookup; mute was not applied");
#endif
                }
                else
                {
                    bSuccess = false;
                    StatusMessage = TEXT("Identity interface not available; mute was not applied");
                }
            }
            else
            {
                bSuccess = false;
                StatusMessage = TEXT("Voice interface not configured; mute was not applied");
            }
        }
        else
        {
            bSuccess = false;
            StatusMessage = TEXT("OnlineSubsystem not available; mute was not applied");
        }
    }
#else
    {
        bSuccess = false;
        StatusMessage = TEXT("No voice system available in this build; mute was not applied");
    }
#endif

    if (!bSuccess)
    {
        SessionsHelpers::StoreLocalVoiceMute(
            TargetIdentifier,
            LocalPlayerNum,
            bSystemWide,
            bMuted);
        bSuccess = true;
        bAppliedToFallbackState = true;
        StatusMessage = FString::Printf(
            TEXT("Native voice interface absent; stored %s in local editor-session mute state"),
            bMuted ? TEXT("mute") : TEXT("unmute"));
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("target"), TargetIdentifier);
    ResponseJson->SetBoolField(TEXT("muted"), bMuted);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetBoolField(TEXT("appliedToVoiceInterface"), bAppliedToVoiceInterface);
    ResponseJson->SetBoolField(TEXT("appliedToFallbackState"), bAppliedToFallbackState);
    ResponseJson->SetNumberField(TEXT("localPlayerNum"), LocalPlayerNum);
    ResponseJson->SetBoolField(TEXT("systemWide"), bSystemWide);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);

    FString Message = FString::Printf(TEXT("Player '%s' %s: %s"),
        *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

#undef MCP_HAS_ONLINE_SUBSYSTEM
#undef MCP_HAS_VOICECHAT
#endif
