#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsProjectConfig.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
// A port is a whole number in [Min, 65535]; anything else (7777.5, 80) is refused.
bool McpReadLanPort(const TSharedPtr<FJsonObject>& Payload, int32 Min, int32& OutPort)
{
    double Value = 0.0;
    if (!Payload.IsValid() || !Payload->TryGetNumberField(TEXT("serverPort"), Value) ||
        Value != FMath::FloorToDouble(Value) || Value < Min || Value > 65535.0)
    {
        return false;
    }
    OutPort = static_cast<int32>(Value);
    return true;
}
}

bool HandleHostLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString MapName = GetJsonStringField(Payload, TEXT("mapName"), TEXT(""));
    int32 MaxPlayers = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxPlayers"), 4));
    FString TravelOptions = GetJsonStringField(Payload, TEXT("travelOptions"), TEXT(""));
    bool bExecuteTravel = GetJsonBoolField(Payload, TEXT("executeTravel"), false);

    if (MapName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("mapName is required to host a LAN server"), nullptr);
        return true;
    }

    FString FullTravelOptions = FString::Printf(TEXT("?listen?bIsLanMatch=1?MaxPlayers=%d"), MaxPlayers);
    if (!TravelOptions.IsEmpty())
    {
        // Each URL option is introduced by '?'; a bare "Foo=1" used to be glued onto MaxPlayers.
        FullTravelOptions += TravelOptions.StartsWith(TEXT("?")) ? TravelOptions : TEXT("?") + TravelOptions;
    }

    FString FullMapPath = MapName;
    if (!FullMapPath.StartsWith(TEXT("/")) && !FullMapPath.Contains(TEXT(":")))
    {
        FullMapPath = FString::Printf(TEXT("/Game/%s"), *MapName);
    }

    FString TravelURL = FullMapPath + FullTravelOptions;
    bool bSuccess = true;
    FString StatusMessage = TEXT("URL built; nothing was hosted (pass executeTravel: true to travel to it)");

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

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("mapName"), MapName);
    ResponseJson->SetStringField(TEXT("mapPath"), FullMapPath);
    ResponseJson->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    ResponseJson->SetStringField(TEXT("travelURL"), TravelURL);
    ResponseJson->SetStringField(TEXT("status"), StatusMessage);
    ResponseJson->SetBoolField(TEXT("travelExecuted"), bExecuteTravel && bSuccess);

    FString Message = FString::Printf(TEXT("LAN listen URL %s: %s"), *TravelURL, *StatusMessage);
    Subsystem->SendAutomationResponse(Socket, RequestId, bSuccess, Message, ResponseJson);
    return true;
}

bool HandleConfigureLanPlay(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    int32 Port = 0;
    if (!McpReadLanPort(Payload, 1024, Port))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("serverPort must be a whole number from 1024 to 65535 (the engine default is 7777); nothing was changed."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // [URL] Port is read into FURL::UrlConfig.DefaultPort at startup: a listen server binds it and a
    // join without an explicit :port connects to it. It is the one persistent LAN setting the engine has.
    const int32 PreviousPort = FURL::UrlConfig.DefaultPort;
    FString ConfigFile;
    FString Error;
    if (!McpHandlerUtils::WriteProjectConfigValue(TEXT("URL"), TEXT("Port"), FString::FromInt(Port), TEXT("Engine"), ConfigFile, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("PERSIST_FAILED"));
        return true;
    }
    FURL::UrlConfig.DefaultPort = Port;

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetNumberField(TEXT("serverPort"), Port);
    ResponseJson->SetNumberField(TEXT("previousPort"), PreviousPort);
    ResponseJson->SetStringField(TEXT("configFile"), ConfigFile);
    ResponseJson->SetBoolField(TEXT("persisted"), true);
    const FString Message = FString::Printf(
        TEXT("Default game port set to %d (was %d): [URL] Port=%d written to %s, so packaged and standalone games listen on and join it from their next launch. ")
        TEXT("The running editor uses it now for host_lan_server and join_lan_server calls without a port. PIE's own listen-server mode keeps the Server Port from Editor Preferences."),
        Port, PreviousPort, Port, *ConfigFile);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleJoinLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Address = GetJsonStringField(Payload, TEXT("serverAddress"), TEXT("")).TrimStartAndEnd();
    int32 AddressBadChar = INDEX_NONE;
    const bool bBadAddress = Address.IsEmpty() || Address.FindChar(TEXT(':'), AddressBadChar) ||
        Address.FindChar(TEXT('?'), AddressBadChar) || Address.FindChar(TEXT('/'), AddressBadChar) ||
        Address.FindChar(TEXT(' '), AddressBadChar);
    if (bBadAddress)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("serverAddress must be a host name or IP such as 127.0.0.1, with the port in serverPort and URL options in travelOptions."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    int32 Port = FURL::UrlConfig.DefaultPort;
    if (Payload->HasField(TEXT("serverPort")) && !McpReadLanPort(Payload, 1, Port))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("serverPort must be a whole number from 1 to 65535; omit it to use the project's default game port."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString Options = GetJsonStringField(Payload, TEXT("travelOptions"), TEXT("")).TrimStartAndEnd();

    // The joining instance must not be the host: a standalone PIE world is preferred (the second
    // instance while the first hosts), then one already connected as a client.
    UWorld* JoinWorld = nullptr;
    int32 JoinInstance = INDEX_NONE;
    bool bAnyPie = false;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            UWorld* World = WorldContext.World();
            if (WorldContext.WorldType != EWorldType::PIE || !World)
            {
                continue;
            }
            bAnyPie = true;
            const ENetMode Mode = World->GetNetMode();
            if (Mode == NM_Standalone || (Mode == NM_Client && !JoinWorld))
            {
                JoinWorld = World;
                JoinInstance = WorldContext.PIEInstance;
                if (Mode == NM_Standalone)
                {
                    break;
                }
            }
        }
    }
    if (!JoinWorld)
    {
        Subsystem->SendAutomationError(Socket, RequestId, bAnyPie
            ? TEXT("Every Play-In-Editor instance is hosting, so none can join. Play with 2 players in Standalone net mode, host on one (host_lan_server executeTravel true), then join.")
            : TEXT("Joining travels a running game: start Play-In-Editor first (control_editor play)."),
            bAnyPie ? TEXT("NO_CLIENT_WORLD") : TEXT("PIE_NOT_RUNNING"));
        return true;
    }
    APlayerController* Controller = JoinWorld->GetFirstPlayerController();
    if (!Controller)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("PIE instance %d has no local player controller to travel."), JoinInstance),
            TEXT("NO_LOCAL_PLAYER"));
        return true;
    }

    FString URL = FString::Printf(TEXT("%s:%d"), *Address, Port);
    if (!Options.IsEmpty())
    {
        URL += Options.StartsWith(TEXT("?")) ? Options : TEXT("?") + Options;
    }
    Controller->ClientTravel(URL, TRAVEL_Absolute);

    const FString WorldName = FString::Printf(TEXT("%s (PIE instance %d)"), *JoinWorld->GetName(), JoinInstance);
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("success"), true);
    ResponseJson->SetStringField(TEXT("connectionURL"), URL);
    ResponseJson->SetStringField(TEXT("pieWorld"), WorldName);
    ResponseJson->SetBoolField(TEXT("travelStarted"), true);
    const FString Message = FString::Printf(
        TEXT("%s is travelling to %s. The connection completes asynchronously: after a few seconds get_sessions_info lists this instance under pieInstances with netMode Client once it joined."),
        *WorldName, *URL);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
