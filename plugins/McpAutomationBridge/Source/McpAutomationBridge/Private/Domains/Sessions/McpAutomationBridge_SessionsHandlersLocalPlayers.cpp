#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"
#include "GameMapsSettings.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"

bool HandleConfigureSplitScreen(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    if (!Payload.IsValid() || (!Payload->HasField(TEXT("enabled")) && !Payload->HasField(TEXT("splitScreenType"))))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("At least one split screen parameter is required (enabled or splitScreenType)"), nullptr);
        return true;
    }

    bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);
    FString SplitScreenType = GetJsonStringField(Payload, TEXT("splitScreenType"), TEXT("TwoPlayer_Horizontal"));
    bool bVerticalSplit = SplitScreenType.Contains(TEXT("Vertical"));
    // Split screen lives on UGameMapsSettings, not UGameUserSettings (dogfood #178: the old code
    // saved the user settings untouched and get_sessions_info never saw the change).
    UGameMapsSettings* MapsSettings = GetMutableDefault<UGameMapsSettings>();
    MapsSettings->bUseSplitscreen = bEnabled;
    MapsSettings->TwoPlayerSplitscreenLayout = bVerticalSplit ? ETwoPlayerSplitScreenType::Vertical : ETwoPlayerSplitScreenType::Horizontal;
    MapsSettings->ThreePlayerSplitscreenLayout = bVerticalSplit ? EThreePlayerSplitScreenType::Vertical : EThreePlayerSplitScreenType::FavorTop;
    MapsSettings->TryUpdateDefaultConfigFile();
    UE_LOG(LogMcpSessionsHandlers, Log, TEXT("Split-screen configured: Enabled=%s, Type=%s"),
        bEnabled ? TEXT("true") : TEXT("false"), *SplitScreenType);

    UGameInstance* GameInstance = GetGameInstance();
    const FString StatusMessage = GameInstance
        ? FString::Printf(TEXT("Split-screen %s with %d local players"),
            bEnabled ? TEXT("configured") : TEXT("disabled"), GameInstance->GetLocalPlayers().Num())
        : FString(TEXT("Split screen settings written to GameMapsSettings (DefaultEngine.ini)"));

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("enabled"), bEnabled);
    ResponseJson->SetStringField(TEXT("splitScreenType"), SplitScreenType);
    ResponseJson->SetBoolField(TEXT("verticalSplit"), bVerticalSplit);
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("settingsSaved"), true);

    FString Message = FString::Printf(TEXT("Split-screen %s with type: %s - %s"),
        bEnabled ? TEXT("enabled") : TEXT("disabled"), *SplitScreenType, *StatusMessage);

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleAddLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    if (!Payload.IsValid() || !Payload->HasField(TEXT("controllerId")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("controllerId is required to add a local player"), nullptr);
        return true;
    }

    int32 ControllerId = static_cast<int32>(GetJsonNumberField(Payload, TEXT("controllerId"), -1));
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;
    }

    FString Error;
    ULocalPlayer* NewPlayer = GameInstance->CreateLocalPlayer(ControllerId, Error, true);
    if (!NewPlayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to add local player: %s"), *Error), nullptr);
        return true;
    }

    int32 PlayerIndex = GameInstance->GetLocalPlayers().Find(NewPlayer);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("playerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("controllerId"), ControllerId);
    ResponseJson->SetNumberField(TEXT("totalLocalPlayers"), GameInstance->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Added local player at index %d (controller ID: %d). Total players: %d"),
        PlayerIndex, ControllerId, GameInstance->GetLocalPlayers().Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleRemoveLocalPlayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace SessionsHelpers;

    if (!Payload.IsValid() || !Payload->HasField(TEXT("playerIndex")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("playerIndex is required to remove a local player"), nullptr);
        return true;
    }

    int32 PlayerIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("playerIndex"), -1));
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No active game instance. Start Play-In-Editor first."), nullptr);
        return true;
    }

    if (PlayerIndex == 0)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Cannot remove the primary local player (index 0)."), nullptr);
        return true;
    }

    ULocalPlayer* Player = GetLocalPlayerByIndex(PlayerIndex);
    if (!Player)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("No local player at index %d"), PlayerIndex), nullptr);
        return true;
    }

    GameInstance->RemoveLocalPlayer(Player);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetNumberField(TEXT("removedPlayerIndex"), PlayerIndex);
    ResponseJson->SetNumberField(TEXT("remainingPlayers"), GameInstance->GetLocalPlayers().Num());

    FString Message = FString::Printf(TEXT("Removed local player at index %d. Remaining players: %d"),
        PlayerIndex, GameInstance->GetLocalPlayers().Num());
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
