#if WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class FOpenCodeAcpClient;

namespace UnrealAgent::AutomationTests
{
    class FScopedIgnoredSigPwrForTest
    {
    public:
        FScopedIgnoredSigPwrForTest();
        ~FScopedIgnoredSigPwrForTest();

    private:
#if PLATFORM_LINUX && defined(SIGPWR)
        struct sigaction PreviousSigPwrAction;
        bool bRestoreSigPwr = false;
#endif
    };

    class FScopedAutomationBridgeSettingsOverride
    {
    public:
        FScopedAutomationBridgeSettingsOverride();
        ~FScopedAutomationBridgeSettingsOverride();

    private:
        bool bHadNativeMcpEnabled = false;
        bool bPreviousNativeMcpEnabled = false;
        bool bHadNativeMcpPort = false;
        int32 PreviousNativeMcpPort = 0;
        bool bHadListenHost = false;
        FString PreviousListenHost;
        bool bHadRequireCapabilityToken = false;
        bool bPreviousRequireCapabilityToken = false;
        bool bHadCapabilityToken = false;
        FString PreviousCapabilityToken;
    };

    FString MakeFakeAcpScript();
    bool PumpClientUntil(FOpenCodeAcpClient& Client, TFunctionRef<bool()> Predicate, double TimeoutSeconds = 5.0);
    bool ContainsTranscript(const TArray<FString>& Entries, const FString& ExpectedText);
}

#endif
