#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sessions/McpAutomationBridge_SessionsHandlersPrivate.h"

namespace SessionsHelpers
{
namespace
{
TSet<FString> LocalVoiceMuteFallbackState;

FString MakeLocalVoiceMuteKey(
    const FString& TargetIdentifier,
    int32 LocalPlayerNum,
    bool bSystemWide)
{
    return FString::Printf(
        TEXT("%s:%d:%s"),
        bSystemWide ? TEXT("system") : TEXT("local"),
        LocalPlayerNum,
        *TargetIdentifier);
}
}

void StoreLocalVoiceMute(
    const FString& TargetIdentifier,
    int32 LocalPlayerNum,
    bool bSystemWide,
    bool bMuted)
{
    const FString Key = MakeLocalVoiceMuteKey(
        TargetIdentifier,
        LocalPlayerNum,
        bSystemWide);
    if (bMuted)
    {
        LocalVoiceMuteFallbackState.Add(Key);
    }
    else
    {
        LocalVoiceMuteFallbackState.Remove(Key);
    }
}
}
