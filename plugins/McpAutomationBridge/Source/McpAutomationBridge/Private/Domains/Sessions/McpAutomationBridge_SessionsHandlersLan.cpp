#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"

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
    if (!FullMapPath.StartsWith(TEXT("/")) && !FullMapPath.Contains(TEXT(":")))
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

