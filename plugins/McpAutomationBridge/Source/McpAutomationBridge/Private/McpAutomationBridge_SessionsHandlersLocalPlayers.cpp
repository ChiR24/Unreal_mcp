#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
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
    bool bSuccess = false;
    FString StatusMessage;

    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings)
    {
        Settings->ApplySettings(false);
        Settings->SaveSettings();

        bSuccess = true;
        StatusMessage = TEXT("Game user settings configured and saved");
        UE_LOG(LogMcpSessionsHandlers, Log, TEXT("Split-screen configured: Enabled=%s, Type=%s"),
            bEnabled ? TEXT("true") : TEXT("false"), *SplitScreenType);
    }
    else
    {
        StatusMessage = TEXT("GameUserSettings not available");
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        int32 CurrentPlayers = GameInstance->GetLocalPlayers().Num();
        bSuccess = true;
        StatusMessage = FString::Printf(TEXT("Split-screen %s with %d local players"),
            bEnabled ? TEXT("configured") : TEXT("disabled"), CurrentPlayers);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
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

bool HandleSetSplitScreenType(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || !Payload->HasField(TEXT("splitScreenType")))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("splitScreenType is required. Valid types: None, TwoPlayer_Horizontal, TwoPlayer_Vertical, ThreePlayer_FavorTop, ThreePlayer_FavorBottom, FourPlayer_Grid"), nullptr);
        return true;
    }

    FString SplitScreenType = GetJsonStringField(Payload, TEXT("splitScreenType"), TEXT("TwoPlayer_Horizontal"));
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
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("splitScreenType"), SplitScreenType);

    FString Message = FString::Printf(TEXT("Split-screen type set to: %s"), *SplitScreenType);
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
#endif
