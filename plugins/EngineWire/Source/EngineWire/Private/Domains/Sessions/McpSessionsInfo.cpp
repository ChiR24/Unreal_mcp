#include "Domains/Sessions/McpSessionsPrivate.h"
#include "GameMapsSettings.h"

#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

bool HandleGetSessionsInfo(
    UEngineWireSubsystem* Subsystem,
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
    SessionsInfo->SetBoolField(TEXT("splitScreenEnabled"), GetDefault<UGameMapsSettings>()->bUseSplitscreen);
    // Whether the Play viewport is split now: that takes the setting, two players or more and no override turning it
    // off (UGameViewportClient::UpdateActiveSplitscreenType), so the player count alone said true with split screen off.
    const UGameViewportClient* Viewport = bInPIE ? GEditor->PlayWorld->GetGameViewport() : nullptr;
    SessionsInfo->SetBoolField(TEXT("splitScreenActive"), Viewport && Viewport->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None);
    // The layout for two, three and four players, each in the name configure_split_screen takes.
    SessionsInfo->SetArrayField(TEXT("splitScreenLayouts"), McpHandlerUtils::ToJsonStringArray(TArray<FString>{
        SessionsHelpers::SplitScreenLayoutName(2), SessionsHelpers::SplitScreenLayoutName(3), SessionsHelpers::SplitScreenLayoutName(4)}));
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
