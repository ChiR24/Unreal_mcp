#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SessionsHandlersPrivate.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

namespace SessionsHelpers
{
UGameInstance* GetGameInstance()
{
    if (GEditor && GEditor->PlayWorld)
    {
        return GEditor->PlayWorld->GetGameInstance();
    }
    return nullptr;
}

ULocalPlayer* GetLocalPlayerByIndex(int32 PlayerIndex)
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
    return LocalPlayers.IsValidIndex(PlayerIndex) ? LocalPlayers[PlayerIndex] : nullptr;
}

int32 GetLocalPlayerCount()
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance ? GameInstance->GetLocalPlayers().Num() : 0;
}
}
#endif
