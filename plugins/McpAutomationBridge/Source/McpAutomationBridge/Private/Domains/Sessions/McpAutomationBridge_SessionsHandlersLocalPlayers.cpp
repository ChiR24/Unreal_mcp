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

namespace
{
// Every named layout the engine has, keyed by player count: each name writes only its own count's field.
struct FMcpSplitScreenLayout
{
    const TCHAR* Name;
    int32 Players;
    uint8 Value;
};
const FMcpSplitScreenLayout GMcpSplitScreenLayouts[] = {
    {TEXT("TwoPlayer_Horizontal"), 2, ETwoPlayerSplitScreenType::Horizontal},
    {TEXT("TwoPlayer_Vertical"), 2, ETwoPlayerSplitScreenType::Vertical},
    {TEXT("ThreePlayer_FavorTop"), 3, EThreePlayerSplitScreenType::FavorTop},
    {TEXT("ThreePlayer_FavorBottom"), 3, EThreePlayerSplitScreenType::FavorBottom},
    {TEXT("ThreePlayer_Vertical"), 3, EThreePlayerSplitScreenType::Vertical},
    {TEXT("ThreePlayer_Horizontal"), 3, EThreePlayerSplitScreenType::Horizontal},
    {TEXT("FourPlayer_Grid"), 4, static_cast<uint8>(EFourPlayerSplitScreenType::Grid)},
    {TEXT("FourPlayer_Vertical"), 4, static_cast<uint8>(EFourPlayerSplitScreenType::Vertical)},
    {TEXT("FourPlayer_Horizontal"), 4, static_cast<uint8>(EFourPlayerSplitScreenType::Horizontal)},
};
}

bool HandleConfigureSplitScreen(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const bool bHasEnabled = Payload.IsValid() && Payload->HasField(TEXT("enabled"));
    const bool bHasType = Payload.IsValid() && Payload->HasField(TEXT("splitScreenType"));
    if (!bHasEnabled && !bHasType)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Pass enabled (turn split screen on or off) or splitScreenType (a layout name, or None)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString TypeName = GetJsonStringField(Payload, TEXT("splitScreenType"), TEXT("")).TrimStartAndEnd();
    const bool bNone = bHasType && TypeName.Equals(TEXT("None"), ESearchCase::IgnoreCase);
    const FMcpSplitScreenLayout* Layout = nullptr;
    FString ValidNames = TEXT("None");
    for (const FMcpSplitScreenLayout& Candidate : GMcpSplitScreenLayouts)
    {
        ValidNames += FString(TEXT(", ")) + Candidate.Name;
        if (TypeName.Equals(Candidate.Name, ESearchCase::IgnoreCase))
        {
            Layout = &Candidate;
        }
    }
    if (bHasType && !bNone && !Layout)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Unknown splitScreenType '%s'; nothing was changed. Use one of: %s."), *TypeName, *ValidNames),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (bNone && GetJsonBoolField(Payload, TEXT("enabled"), false))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("splitScreenType None turns split screen off, which contradicts enabled true; pick a layout name instead."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const bool bEnabled = !bNone && GetJsonBoolField(Payload, TEXT("enabled"), true);

    // Split screen lives on UGameMapsSettings (DefaultEngine.ini), which the game viewport reads every
    // frame, so the change is live in PIE and ships with the game.
    UGameMapsSettings* MapsSettings = GetMutableDefault<UGameMapsSettings>();
    const bool bPreviousEnabled = MapsSettings->bUseSplitscreen;
    const TEnumAsByte<ETwoPlayerSplitScreenType::Type> PreviousTwo = MapsSettings->TwoPlayerSplitscreenLayout;
    const TEnumAsByte<EThreePlayerSplitScreenType::Type> PreviousThree = MapsSettings->ThreePlayerSplitscreenLayout;
    const EFourPlayerSplitScreenType PreviousFour = MapsSettings->FourPlayerSplitscreenLayout;

    MapsSettings->bUseSplitscreen = bEnabled;
    FString LayoutWritten;
    if (Layout && Layout->Players == 2)
    {
        MapsSettings->TwoPlayerSplitscreenLayout = static_cast<ETwoPlayerSplitScreenType::Type>(Layout->Value);
        LayoutWritten = TEXT("TwoPlayerSplitscreenLayout");
    }
    else if (Layout && Layout->Players == 3)
    {
        MapsSettings->ThreePlayerSplitscreenLayout = static_cast<EThreePlayerSplitScreenType::Type>(Layout->Value);
        LayoutWritten = TEXT("ThreePlayerSplitscreenLayout");
    }
    else if (Layout)
    {
        MapsSettings->FourPlayerSplitscreenLayout = static_cast<EFourPlayerSplitScreenType>(Layout->Value);
        LayoutWritten = TEXT("FourPlayerSplitscreenLayout");
    }
    if (!MapsSettings->TryUpdateDefaultConfigFile())
    {
        MapsSettings->bUseSplitscreen = bPreviousEnabled;
        MapsSettings->TwoPlayerSplitscreenLayout = PreviousTwo;
        MapsSettings->ThreePlayerSplitscreenLayout = PreviousThree;
        MapsSettings->FourPlayerSplitscreenLayout = PreviousFour;
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("DefaultEngine.ini could not be written (read-only, or under source control without a checkout); the split-screen settings were restored."),
            TEXT("PERSIST_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetBoolField(TEXT("enabled"), bEnabled);
    ResponseJson->SetStringField(TEXT("splitScreenType"), Layout ? FString(Layout->Name) : (bNone ? FString(TEXT("None")) : FString()));
    ResponseJson->SetStringField(TEXT("layoutsWritten"), LayoutWritten);
    ResponseJson->SetBoolField(TEXT("settingsSaved"), true);
    const FString LayoutNote = Layout
        ? FString::Printf(TEXT(", %s = %s (other player counts unchanged)"), *LayoutWritten, Layout->Name)
        : FString();
    const FString Message = FString::Printf(
        TEXT("Split screen %s%s: written to DefaultEngine.ini (GameMapsSettings) and applied to the running editor and PIE at once."),
        bEnabled ? TEXT("on") : TEXT("off"), *LayoutNote);
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
