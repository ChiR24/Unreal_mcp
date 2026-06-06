#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"

bool HandleConfigureLanPlay(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!Payload.IsValid() || (!Payload->HasField(TEXT("enabled")) && !Payload->HasField(TEXT("serverPort")) && !Payload->HasField(TEXT("serverPassword"))))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("At least one LAN play parameter is required (enabled, serverPort, or serverPassword)"), nullptr);
        return true;
    }

    bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);
    int32 ServerPort = static_cast<int32>(GetJsonNumberField(Payload, TEXT("serverPort"), 7777));
    FString ServerPassword = GetJsonStringField(Payload, TEXT("serverPassword"), TEXT(""));

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
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

bool HandleHostLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ServerName = GetJsonStringField(Payload, TEXT("serverName"), TEXT("LAN Server"));
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
        FullTravelOptions += TravelOptions;
    }

    FString FullMapPath = MapName;
    if (!FullMapPath.StartsWith(TEXT("/Game/")) && !FullMapPath.StartsWith(TEXT("/")) && !FullMapPath.Contains(TEXT(":")))
    {
        FullMapPath = FString::Printf(TEXT("/Game/%s"), *MapName);
    }

    FString TravelURL = FullMapPath + FullTravelOptions;
    bool bSuccess = true;
    FString StatusMessage = TEXT("configured");

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

bool HandleJoinLanServer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ServerAddress = GetJsonStringField(Payload, TEXT("serverAddress"), TEXT(""));
    int32 ServerPort = static_cast<int32>(GetJsonNumberField(Payload, TEXT("serverPort"), 7777));
    FString ServerPassword = GetJsonStringField(Payload, TEXT("serverPassword"), TEXT(""));
    FString TravelOptions = GetJsonStringField(Payload, TEXT("travelOptions"), TEXT(""));

    if (ServerAddress.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("serverAddress is required to join a LAN server"), nullptr);
        return true;
    }

    FString ConnectionString = FString::Printf(TEXT("%s:%d"), *ServerAddress, ServerPort);
    if (!ServerPassword.IsEmpty())
    {
        TravelOptions += FString::Printf(TEXT("?Password=%s"), *ServerPassword);
    }
    FString FullURL = ConnectionString + TravelOptions;

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("serverAddress"), ConnectionString);
    ResponseJson->SetStringField(TEXT("connectionURL"), FullURL);
    ResponseJson->SetStringField(TEXT("status"), TEXT("configured"));

    FString Message = FString::Printf(TEXT("Configured to join LAN server at %s. Use ClientTravel to connect."),
        *ConnectionString);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
#endif
