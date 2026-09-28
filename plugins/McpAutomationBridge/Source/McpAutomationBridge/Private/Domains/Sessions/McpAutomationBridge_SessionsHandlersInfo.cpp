#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"
#include "GameMapsSettings.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

bool HandleGetSessionsInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetBoolField(TEXT("success"), true);
    TSharedPtr<FJsonObject> SessionsInfo = McpHandlerUtils::CreateResultObject();

    int32 LocalPlayerCount = SessionsHelpers::GetLocalPlayerCount();
    SessionsInfo->SetNumberField(TEXT("localPlayerCount"), LocalPlayerCount);

    bool bInPIE = GEditor && GEditor->PlayWorld != nullptr;
    SessionsInfo->SetBoolField(TEXT("inPlaySession"), bInPIE);
    SessionsInfo->SetNumberField(TEXT("currentPlayers"), LocalPlayerCount);
    const UGameMapsSettings* MapsSettings = GetDefault<UGameMapsSettings>();
    const bool bSplitScreenConfigured = MapsSettings && MapsSettings->bUseSplitscreen;
    SessionsInfo->SetBoolField(TEXT("splitScreenEnabled"), bSplitScreenConfigured);
    SessionsInfo->SetBoolField(TEXT("splitScreenActive"), LocalPlayerCount > 1);
    SessionsInfo->SetStringField(TEXT("splitScreenLayout"), MapsSettings ? StaticEnum<ETwoPlayerSplitScreenType::Type>()->GetNameStringByValue(static_cast<int64>(MapsSettings->TwoPlayerSplitscreenLayout.GetValue())) : TEXT("Unknown"));
    // Per PIE instance net mode: the evidence that host_lan_server made a ListenServer and that
    // join_lan_server connected a Client.
    TArray<TSharedPtr<FJsonValue>> PieInstances;
    static const TCHAR* const NetModeNames[] = {TEXT("Standalone"), TEXT("DedicatedServer"), TEXT("ListenServer"), TEXT("Client")};
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            const UWorld* World = WorldContext.World();
            if (WorldContext.WorldType != EWorldType::PIE || !World)
            {
                continue;
            }
            const int32 Mode = static_cast<int32>(World->GetNetMode());
            TSharedPtr<FJsonObject> Instance = MakeShared<FJsonObject>();
            Instance->SetNumberField(TEXT("instance"), WorldContext.PIEInstance);
            Instance->SetStringField(TEXT("netMode"), Mode >= 0 && Mode < 4 ? NetModeNames[Mode] : TEXT("Unknown"));
            Instance->SetStringField(TEXT("url"), World->URL.ToString());
            PieInstances.Add(MakeShared<FJsonValueObject>(Instance));
        }
    }
    SessionsInfo->SetArrayField(TEXT("pieInstances"), PieInstances);
    ResponseJson->SetObjectField(TEXT("sessionsInfo"), SessionsInfo);

    FString Message = FString::Printf(TEXT("Sessions info retrieved. Local players: %d, In PIE: %s"),
        LocalPlayerCount, bInPIE ? TEXT("Yes") : TEXT("No"));
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}
