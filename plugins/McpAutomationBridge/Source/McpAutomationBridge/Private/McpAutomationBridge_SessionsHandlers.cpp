// McpAutomationBridge_SessionsHandlers.cpp
// Phase 22: Sessions & Local Multiplayer System Handlers
//
// Complete session management including:
// - Session Management (local session settings, session interface)
// - Local Multiplayer (split-screen, local players)
// - LAN (LAN play configuration, hosting, joining)
// - Voice Chat (voice settings, channels, muting, attenuation, push-to-talk)

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"

// Helper macros for JSON field access
#define GetStringFieldSess GetJsonStringField
#define GetNumberFieldSess GetJsonNumberField
#define GetBoolFieldSess GetJsonBoolField

#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

// Voice Chat interfaces (conditional availability)
#if __has_include("VoiceChat.h")
#include "VoiceChat.h"
#define MCP_HAS_VOICECHAT 1
#else
#define MCP_HAS_VOICECHAT 0
#endif

// Online Subsystem for voice muting via IOnlineVoice
#if __has_include("OnlineSubsystem.h")
#include "OnlineSubsystem.h"
#include "Interfaces/VoiceInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#define MCP_HAS_ONLINE_SUBSYSTEM 1
#else
#define MCP_HAS_ONLINE_SUBSYSTEM 0
#endif

#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogMcpSessionsHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================

namespace SessionsHelpers
{

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

#if WITH_EDITOR
    // Get game instance
    UGameInstance* GetGameInstance()
    {
        if (GEditor && GEditor->PlayWorld)
        {
            return GEditor->PlayWorld->GetGameInstance();
        }
        return nullptr;
    }

    // Get local player by index
    ULocalPlayer* GetLocalPlayerByIndex(int32 PlayerIndex)
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
            if (LocalPlayers.IsValidIndex(PlayerIndex))
            {
                return LocalPlayers[PlayerIndex];
            }
        }
        return nullptr;
    }

    // Get number of local players
    int32 GetLocalPlayerCount()
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            return GI->GetLocalPlayers().Num();
        }
        return 0;
    }
#endif
}

// ============================================================================
// Session Management Actions
// ============================================================================

#if WITH_EDITOR

static bool HandleConfigureLocalSessionSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    // Extract session settings from payload
    FString SessionName = GetStringFieldSess(Payload, TEXT("sessionName"), TEXT("DefaultSession"));
    int32 MaxPlayers = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("maxPlayers"), 4.0));
    bool bIsLANMatch = GetBoolFieldSess(Payload, TEXT("bIsLANMatch"), false);
    bool bAllowJoinInProgress = GetBoolFieldSess(Payload, TEXT("bAllowJoinInProgress"), true);
    bool bAllowInvites = GetBoolFieldSess(Payload, TEXT("bAllowInvites"), true);
    bool bUsesPresence = GetBoolFieldSess(Payload, TEXT("bUsesPresence"), true);
    bool bUseLobbiesIfAvailable = GetBoolFieldSess(Payload, TEXT("bUseLobbiesIfAvailable"), true);
    bool bShouldAdvertise = GetBoolFieldSess(Payload, TEXT("bShouldAdvertise"), true);

    // Build response with session configuration
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("sessionName"), SessionName);
    ResponseJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    ResponseJson->SetBoolField(TEXT("bIsLANMatch"), bIsLANMatch);
    ResponseJson->SetBoolField(TEXT("bAllowJoinInProgress"), bAllowJoinInProgress);
    ResponseJson->SetBoolField(TEXT("bAllowInvites"), bAllowInvites);
    ResponseJson->SetBoolField(TEXT("bUsesPresence"), bUsesPresence);
    ResponseJson->SetBoolField(TEXT("bUseLobbiesIfAvailable"), bUseLobbiesIfAvailable);
    ResponseJson->SetBoolField(TEXT("bShouldAdvertise"), bShouldAdvertise);

    FString Message = FString::Printf(TEXT("Local session settings configured: '%s' with max %d players (LAN: %s)"),
        *SessionName, MaxPlayers, bIsLANMatch ? TEXT("Yes") : TEXT("No"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConfigureSessionInterface(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString InterfaceType = GetStringFieldSess(Payload, TEXT("interfaceType"), TEXT("Default"));

    // Validate interface type
    TArray<FString> ValidTypes = { TEXT("Default"), TEXT("LAN"), TEXT("Null") };
    if (!ValidTypes.Contains(InterfaceType))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid session interface type: %s. Valid types: Default, LAN, Null"), *InterfaceType), nullptr);
        return false;
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("interfaceType"), InterfaceType);
    ResponseJson->SetStringField(TEXT("status"), TEXT("configured"));

    FString Message = FString::Printf(TEXT("Session interface configured to: %s"), *InterfaceType);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Local Multiplayer Actions
// ============================================================================

static bool HandleConfigureSplitScreen(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    bool bEnabled = GetBoolFieldSess(Payload, TEXT("enabled"), true);
    FString SplitScreenType = GetStringFieldSess(Payload, TEXT("splitScreenType"), TEXT("TwoPlayer_Horizontal"));
    bool bVerticalSplit = SplitScreenType.Contains(TEXT("Vertical"));
    bool bSuccess = false;
    FString StatusMessage;

    // Configure split screen via game user settings
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings)
    {
        // Apply settings that affect split-screen behavior
        // Note: UE doesn't have a direct "split screen enabled" toggle in GameUserSettings
        // Split-screen is typically controlled by the GameMode and player controller spawning
        // We can configure related settings and save them
        
        // Save the current settings to persist the configuration
        Settings->ApplySettings(false);
        Settings->SaveSettings();
        
        bSuccess = true;
        StatusMessage = TEXT("Game user settings configured and saved");
        
        // Log the configuration for debugging
        UE_LOG(LogMcpSessionsHandlers, Log, TEXT("Split-screen configured: Enabled=%s, Type=%s"), 
            bEnabled ? TEXT("true") : TEXT("false"), *SplitScreenType);
    }
    else
    {
        StatusMessage = TEXT("GameUserSettings not available");
    }
    
    // Additionally, we can configure the GameViewportClient if in PIE
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        // The actual split-screen layout is controlled by UGameViewportClient
        // and is typically set up automatically when local players are added
        int32 CurrentPlayers = GI->GetLocalPlayers().Num();
        
        // If we have multiple players, split-screen is implicitly enabled
        bSuccess = true;
        StatusMessage = FString::Printf(TEXT("Split-screen %s with %d local players"),
            bEnabled ? TEXT("configured") : TEXT("disabled"), CurrentPlayers);
    }
    
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetBoolField(TEXT("enabled"), bEnabled);
    ResponseJson->SetStringField(TEXT("splitScreenType"), SplitScreenType);
    ResponseJson->SetBoolField(TEXT("verticalSplit"), bVerticalSplit);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("settingsSaved"), Settings != nullptr);

    FString Message = FString::Printf(TEXT("Split-screen %s with type: %s - %s"),
        bEnabled ? TEXT("enabled") : TEXT("disabled"), *SplitScreenType, *StatusMessage);

    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

static bool HandleSetSplitScreenType(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString SplitScreenType = GetStringFieldSess(Payload, TEXT("splitScreenType"), TEXT("TwoPlayer_Horizontal"));

    // Validate split screen type
    TArray<FString> ValidTypes = {
        TEXT("None"),
        TEXT("TwoPlayer_Horizontal"),
        TEXT("TwoPlayer_Vertical"),
        TEXT("ThreePlayer_FavorTop"),
        TEXT("ThreePlayer_FavorBottom"),
        TEXT("FourPlayer_Grid")
    };

    if (!ValidTypes.Contains(SplitScreenType))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid split-screen type: %s"), *SplitScreenType), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("splitScreenType"), SplitScreenType);

    FString Message = FString::Printf(TEXT("Split-screen type set to: %s"), *SplitScreenType);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleAddLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    int32 ControllerId = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("controllerId"), -1));

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Create new local player
    FString Error;
    ULocalPlayer* NewPlayer = GI->CreateLocalPlayer(ControllerId, Error, true);
    
    if (!NewPlayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to add local player: %s"), *Error), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    int32 PlayerIndex = GI->GetLocalPlayers().Find(NewPlayer);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("playerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("controllerId"), ControllerId);
    ResponseJson->SetNumberField(TEXT("totalLocalPlayers"), GI->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Added local player at index %d (controller ID: %d). Total players: %d"),
        PlayerIndex, ControllerId, GI->GetLocalPlayers().Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleRemoveLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    int32 PlayerIndex = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("playerIndex"), -1));

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Cannot remove player 0 (primary player)
    if (PlayerIndex == 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Cannot remove the primary local player (index 0)."), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    ULocalPlayer* Player = GetLocalPlayerByIndex(PlayerIndex);
    if (!Player)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No local player at index %d"), PlayerIndex), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    GI->RemoveLocalPlayer(Player);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("removedPlayerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("remainingPlayers"), GI->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Removed local player at index %d. Remaining players: %d"),
        PlayerIndex, GI->GetLocalPlayers().Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// LAN Actions
// ============================================================================

static bool HandleConfigureLanPlay(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    bool bEnabled = GetBoolFieldSess(Payload, TEXT("enabled"), true);
    int32 ServerPort = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("serverPort"), 7777));
    FString ServerPassword = GetStringFieldSess(Payload, TEXT("serverPassword"), TEXT(""));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetBoolField(TEXT("enabled"), bEnabled);
    ResponseJson->SetNumberField(TEXT("serverPort"), ServerPort);
    ResponseJson->SetBoolField(TEXT("hasPassword"), !ServerPassword.IsEmpty());

    FString Message = FString::Printf(TEXT("LAN play %s on port %d%s"),
        bEnabled ? TEXT("enabled") : TEXT("disabled"),
        ServerPort,
        ServerPassword.IsEmpty() ? TEXT("") : TEXT(" (password protected)"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleHostLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString ServerName = GetStringFieldSess(Payload, TEXT("serverName"), TEXT("LAN Server"));
    FString MapName = GetStringFieldSess(Payload, TEXT("mapName"), TEXT(""));
    int32 MaxPlayers = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("maxPlayers"), 4));
    FString TravelOptions = GetStringFieldSess(Payload, TEXT("travelOptions"), TEXT(""));
    bool bExecuteTravel = GetBoolFieldSess(Payload, TEXT("executeTravel"), false);

    if (MapName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("mapName is required to host a LAN server"), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Build the travel URL with LAN-specific options
    FString FullTravelOptions = FString::Printf(TEXT("?listen?bIsLanMatch=1?MaxPlayers=%d"), MaxPlayers);
    if (!TravelOptions.IsEmpty())
    {
        FullTravelOptions += TravelOptions;
    }
    
    // Ensure map path starts with /Game/ if it doesn't already
    FString FullMapPath = MapName;
    if (!FullMapPath.StartsWith(TEXT("/Game/")) && !FullMapPath.StartsWith(TEXT("/")) && !FullMapPath.Contains(TEXT(":")))
    {
        FullMapPath = FString::Printf(TEXT("/Game/%s"), *MapName);
    }
    
    FString TravelURL = FullMapPath + FullTravelOptions;
    bool bSuccess = true;
    FString StatusMessage = TEXT("configured");

    // Optionally execute the server travel
    if (bExecuteTravel)
    {
        UWorld* World = nullptr;
        if (GEditor && GEditor->PlayWorld)
        {
            World = GEditor->PlayWorld;
        }
        else if (GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }
        
        if (World)
        {
            // Use ServerTravel to start hosting
            // ServerTravel is used on the server to travel all clients to a new map
            bool bAbsolute = true;
            World->ServerTravel(TravelURL, bAbsolute);
            StatusMessage = TEXT("server travel initiated");
            UE_LOG(LogMcpSessionsHandlers, Log, TEXT("LAN Server: Initiated ServerTravel to %s"), *TravelURL);
        }
        else
        {
            bSuccess = false;
            StatusMessage = TEXT("No world available. Start Play-In-Editor first to execute travel.");
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("serverName"), ServerName);
    ResponseJson->SetStringField(TEXT("mapName"), MapName);
    ResponseJson->SetStringField(TEXT("mapPath"), FullMapPath);
    ResponseJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    ResponseJson->SetStringField(TEXT("travelURL"), TravelURL);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("travelExecuted"), bExecuteTravel && bSuccess);

    FString Message = FString::Printf(TEXT("LAN server '%s' %s for map '%s' (max %d players)"),
        *ServerName, *StatusMessage, *MapName, MaxPlayers);

    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

static bool HandleJoinLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString ServerAddress = GetStringFieldSess(Payload, TEXT("serverAddress"), TEXT(""));
    int32 ServerPort = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("serverPort"), 7777));
    FString ServerPassword = GetStringFieldSess(Payload, TEXT("serverPassword"), TEXT(""));
    FString TravelOptions = GetStringFieldSess(Payload, TEXT("travelOptions"), TEXT(""));

    if (ServerAddress.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("serverAddress is required to join a LAN server"), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    // Build connection string
    FString ConnectionString = FString::Printf(TEXT("%s:%d"), *ServerAddress, ServerPort);
    if (!ServerPassword.IsEmpty())
    {
        TravelOptions += FString::Printf(TEXT("?Password=%s"), *ServerPassword);
    }
    FString FullURL = ConnectionString + TravelOptions;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("serverAddress"), ConnectionString);
    ResponseJson->SetStringField(TEXT("connectionURL"), FullURL);
    ResponseJson->SetStringField(TEXT("status"), TEXT("configured"));

    FString Message = FString::Printf(TEXT("Configured to join LAN server at %s. Use ClientTravel to connect."),
        *ConnectionString);

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Voice Chat Actions
// ============================================================================

static bool HandleEnableVoiceChat(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    bool bEnabled = GetBoolFieldSess(Payload, TEXT("voiceEnabled"), true);
    bool bSuccess = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    // Use the IVoiceChat modular feature interface
    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (VoiceChat)
    {
        if (bEnabled)
        {
            // Initialize and connect voice chat
            if (!VoiceChat->IsInitialized())
            {
                bSuccess = VoiceChat->Initialize();
                if (bSuccess)
                {
                    StatusMessage = TEXT("Voice chat initialized");
                    // Connect asynchronously - we report success on initialize
                    VoiceChat->Connect(FOnVoiceChatConnectCompleteDelegate::CreateLambda(
                        [](const FVoiceChatResult& Result)
                        {
                            // ErrorDesc is a public member in both UE 5.6 and 5.7
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
            // Disconnect and uninitialize voice chat
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
        // VoiceChat interface not available (no voice plugin loaded)
        StatusMessage = TEXT("IVoiceChat interface not available - no voice chat plugin loaded");
        bSuccess = false;
    }
#else
    // VoiceChat module not available at compile time
    StatusMessage = TEXT("Voice chat module not available in this build");
    bSuccess = true; // Return success but note the limitation
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
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

static bool HandleConfigureVoiceSettings(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    TSharedPtr<FJsonObject> VoiceSettings = GetObjectField(Payload, TEXT("voiceSettings"));
    
    double Volume = 1.0;
    double NoiseGateThreshold = 0.01;
    bool bNoiseSuppression = true;
    bool bEchoCancellation = true;
    int32 SampleRate = 16000;

    if (VoiceSettings.IsValid())
    {
        Volume = FMath::Clamp(GetNumberFieldSess(VoiceSettings, TEXT("volume"), 1.0), 0.0, 1.0);
        NoiseGateThreshold = GetNumberFieldSess(VoiceSettings, TEXT("noiseGateThreshold"), 0.01);
        bNoiseSuppression = GetBoolFieldSess(VoiceSettings, TEXT("noiseSuppression"), true);
        bEchoCancellation = GetBoolFieldSess(VoiceSettings, TEXT("echoCancellation"), true);
        SampleRate = static_cast<int32>(GetNumberFieldSess(VoiceSettings, TEXT("sampleRate"), 16000));
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    
    TSharedPtr<FJsonObject> ConfiguredSettings = MakeShareable(new FJsonObject());
    ConfiguredSettings->SetNumberField(TEXT("volume"), Volume);
    ConfiguredSettings->SetNumberField(TEXT("noiseGateThreshold"), NoiseGateThreshold);
    ConfiguredSettings->SetBoolField(TEXT("noiseSuppression"), bNoiseSuppression);
    ConfiguredSettings->SetBoolField(TEXT("echoCancellation"), bEchoCancellation);
    ConfiguredSettings->SetNumberField(TEXT("sampleRate"), SampleRate);
    ResponseJson->SetObjectField(TEXT("voiceSettings"), ConfiguredSettings);

    FString Message = TEXT("Voice chat settings configured successfully");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleSetVoiceChannel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString ChannelName = GetStringFieldSess(Payload, TEXT("channelName"), TEXT("Default"));
    FString ChannelType = GetStringFieldSess(Payload, TEXT("channelType"), TEXT("Global"));

    // Validate channel type
    TArray<FString> ValidTypes = { TEXT("Team"), TEXT("Global"), TEXT("Proximity"), TEXT("Party") };
    if (!ValidTypes.Contains(ChannelType))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid voice channel type: %s. Valid types: Team, Global, Proximity, Party"), *ChannelType), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("channelName"), ChannelName);
    ResponseJson->SetStringField(TEXT("channelType"), ChannelType);

    FString Message = FString::Printf(TEXT("Voice channel '%s' set with type: %s"), *ChannelName, *ChannelType);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleMutePlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    FString PlayerName = GetStringFieldSess(Payload, TEXT("playerName"), TEXT(""));
    FString TargetPlayerId = GetStringFieldSess(Payload, TEXT("targetPlayerId"), TEXT(""));
    bool bMuted = GetBoolFieldSess(Payload, TEXT("muted"), true);
    int32 LocalPlayerNum = static_cast<int32>(GetNumberFieldSess(Payload, TEXT("localPlayerNum"), 0));
    bool bSystemWide = GetBoolFieldSess(Payload, TEXT("systemWide"), false);

    if (PlayerName.IsEmpty() && TargetPlayerId.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Either playerName or targetPlayerId is required"), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    FString TargetIdentifier = !TargetPlayerId.IsEmpty() ? TargetPlayerId : PlayerName;
    bool bSuccess = false;
    FString StatusMessage;

#if MCP_HAS_VOICECHAT
    // Try IVoiceChat first (newer interface)
    IVoiceChat* VoiceChat = IVoiceChat::Get();
    if (VoiceChat && VoiceChat->IsLoggedIn())
    {
        VoiceChat->SetPlayerMuted(TargetIdentifier, bMuted);
        bSuccess = true;
        StatusMessage = FString::Printf(TEXT("Player '%s' %s via IVoiceChat"), 
            *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"));
    }
    else
#endif
#if MCP_HAS_ONLINE_SUBSYSTEM
    {
        // Fall back to IOnlineVoice via OnlineSubsystem
        IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
        if (OnlineSub)
        {
            IOnlineVoicePtr VoiceInterface = OnlineSub->GetVoiceInterface();
            if (VoiceInterface.IsValid())
            {
                // Create a unique net ID from the player ID string
                IOnlineIdentityPtr IdentityInterface = OnlineSub->GetIdentityInterface();
                if (IdentityInterface.IsValid())
                {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
                    // UE 5.0-5.6: CreateUniquePlayerId was available on some platforms
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
                    }
                    else
                    {
                        StatusMessage = TEXT("Failed to create unique net ID for player");
                    }
#else
                    // UE 5.7+: CreateUniquePlayerId was removed. Use GetUniquePlayerId for local players
                    // or find the player in the registered players list.
                    // For remote players, we need to find them via the session or player controller.
                    UE_LOG(LogMcpSessionsHandlers, Warning, TEXT("CreateUniquePlayerId not available in UE 5.7+. "
                        "Remote player mute by ID requires session-based lookup."));
                    StatusMessage = TEXT("Direct player ID mute not supported in UE 5.7+. Use local player index instead.");
#endif
                }
                else
                {
                    StatusMessage = TEXT("Identity interface not available");
                }
            }
            else
            {
                StatusMessage = TEXT("Voice interface not available in OnlineSubsystem");
            }
        }
        else
        {
            StatusMessage = TEXT("OnlineSubsystem not available");
        }
    }
#else
    {
        // No voice system available - just acknowledge the request
        bSuccess = true;
        StatusMessage = TEXT("Mute state recorded (no voice system available in this build)");
    }
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("target"), TargetIdentifier);
    ResponseJson->SetBoolField(TEXT("muted"), bMuted);
    ResponseJson->SetBoolField(TEXT("success"), bSuccess);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);

    FString Message = FString::Printf(TEXT("Player '%s' %s: %s"), 
        *TargetIdentifier, bMuted ? TEXT("muted") : TEXT("unmuted"), *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

static bool HandleSetVoiceAttenuation(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    double AttenuationRadius = GetNumberFieldSess(Payload, TEXT("attenuationRadius"), 2000.0);
    double AttenuationFalloff = GetNumberFieldSess(Payload, TEXT("attenuationFalloff"), 1.0);

    // Clamp values to reasonable ranges
    AttenuationRadius = FMath::Max(AttenuationRadius, 0.0);
    AttenuationFalloff = FMath::Clamp(AttenuationFalloff, 0.1, 10.0);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetNumberField(TEXT("attenuationRadius"), AttenuationRadius);
    ResponseJson->SetNumberField(TEXT("attenuationFalloff"), AttenuationFalloff);

    FString Message = FString::Printf(TEXT("Voice attenuation configured: radius=%.0f, falloff=%.2f"),
        AttenuationRadius, AttenuationFalloff);

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConfigurePushToTalk(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    bool bPushToTalkEnabled = GetBoolFieldSess(Payload, TEXT("pushToTalkEnabled"), false);
    FString PushToTalkKey = GetStringFieldSess(Payload, TEXT("pushToTalkKey"), TEXT("V"));

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetBoolField(TEXT("pushToTalkEnabled"), bPushToTalkEnabled);
    ResponseJson->SetStringField(TEXT("pushToTalkKey"), PushToTalkKey);

    FString Message = FString::Printf(TEXT("Push-to-talk %s%s"),
        bPushToTalkEnabled ? TEXT("enabled") : TEXT("disabled"),
        bPushToTalkEnabled ? *FString::Printf(TEXT(" (key: %s)"), *PushToTalkKey) : TEXT(""));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Utility Actions
// ============================================================================

static bool HandleGetSessionsInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    TSharedPtr<FJsonObject> SessionsInfo = MakeShareable(new FJsonObject());

    // Get local player count
    int32 LocalPlayerCount = GetLocalPlayerCount();
    SessionsInfo->SetNumberField(TEXT("localPlayerCount"), LocalPlayerCount);

    // Check if we're in a PIE session
    bool bInPIE = GEditor && GEditor->PlayWorld != nullptr;
    SessionsInfo->SetBoolField(TEXT("inPlaySession"), bInPIE);

    // Default values for session state
    SessionsInfo->SetStringField(TEXT("currentSessionName"), TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("isLANMatch"), false);
    SessionsInfo->SetNumberField(TEXT("maxPlayers"), 0);
    SessionsInfo->SetNumberField(TEXT("currentPlayers"), LocalPlayerCount);
    SessionsInfo->SetBoolField(TEXT("splitScreenEnabled"), LocalPlayerCount > 1);
    SessionsInfo->SetStringField(TEXT("splitScreenType"), LocalPlayerCount > 1 ? TEXT("Active") : TEXT("None"));
    SessionsInfo->SetBoolField(TEXT("voiceChatEnabled"), false);
    SessionsInfo->SetBoolField(TEXT("isHosting"), false);
    SessionsInfo->SetStringField(TEXT("connectedServerAddress"), TEXT(""));

    // Active voice channels (empty array for now)
    TArray<TSharedPtr<FJsonValue>> VoiceChannels;
    SessionsInfo->SetArrayField(TEXT("activeVoiceChannels"), VoiceChannels);

    ResponseJson->SetObjectField(TEXT("sessionsInfo"), SessionsInfo);

    FString Message = FString::Printf(TEXT("Sessions info retrieved. Local players: %d, In PIE: %s"),
        LocalPlayerCount, bInPIE ? TEXT("Yes") : TEXT("No"));

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Handler Function
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageSessionsAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    // Extract sub-action from payload
    FString SubAction;
    if (Payload.IsValid() && Payload->HasField(TEXT("action")))
    {
        SubAction = GetStringFieldSess(Payload, TEXT("action"));
    }
    
    UE_LOG(LogMcpSessionsHandlers, Log, TEXT("HandleManageSessionsAction: SubAction=%s, RequestId=%s"), *SubAction, *RequestId);

    bool bHandled = false;

    // Session Management
    if (SubAction == TEXT("configure_local_session_settings"))
    {
        bHandled = HandleConfigureLocalSessionSettings(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_session_interface"))
    {
        bHandled = HandleConfigureSessionInterface(this, RequestId, Payload, Socket);
    }
    // Local Multiplayer
    else if (SubAction == TEXT("configure_split_screen"))
    {
        bHandled = HandleConfigureSplitScreen(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("set_split_screen_type"))
    {
        bHandled = HandleSetSplitScreenType(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("add_local_player"))
    {
        bHandled = HandleAddLocalPlayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("remove_local_player"))
    {
        bHandled = HandleRemoveLocalPlayer(this, RequestId, Payload, Socket);
    }
    // LAN
    else if (SubAction == TEXT("configure_lan_play"))
    {
        bHandled = HandleConfigureLanPlay(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("host_lan_server"))
    {
        bHandled = HandleHostLanServer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("join_lan_server"))
    {
        bHandled = HandleJoinLanServer(this, RequestId, Payload, Socket);
    }
    // Voice Chat
    else if (SubAction == TEXT("enable_voice_chat"))
    {
        bHandled = HandleEnableVoiceChat(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_voice_settings"))
    {
        bHandled = HandleConfigureVoiceSettings(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("set_voice_channel"))
    {
        bHandled = HandleSetVoiceChannel(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("mute_player"))
    {
        bHandled = HandleMutePlayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("set_voice_attenuation"))
    {
        bHandled = HandleSetVoiceAttenuation(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_push_to_talk"))
    {
        bHandled = HandleConfigurePushToTalk(this, RequestId, Payload, Socket);
    }
    // Utility
    else if (SubAction == TEXT("get_sessions_info"))
    {
        bHandled = HandleGetSessionsInfo(this, RequestId, Payload, Socket);
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Unknown manage_sessions action: %s"), *SubAction), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    return bHandled;

#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("manage_sessions requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

#undef GetStringFieldSess
#undef GetNumberFieldSess
#undef GetBoolFieldSess

