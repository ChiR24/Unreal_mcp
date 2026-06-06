#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/McpOpenCodeAcpClient.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

namespace
{
    constexpr const TCHAR* TestAutomationBridgeSettingsSection = TEXT("/Script/McpAutomationBridge.McpAutomationBridgeSettings");
}

namespace UnrealAgent::AutomationTests
{
    FScopedIgnoredSigPwrForTest::FScopedIgnoredSigPwrForTest()
    {
#if PLATFORM_LINUX && defined(SIGPWR)
        if (sigaction(SIGPWR, nullptr, &PreviousSigPwrAction) == 0)
        {
            struct sigaction IgnoreAction;
            FMemory::Memzero(&IgnoreAction, sizeof(IgnoreAction));
            IgnoreAction.sa_handler = SIG_IGN;
            sigemptyset(&IgnoreAction.sa_mask);
            bRestoreSigPwr = sigaction(SIGPWR, &IgnoreAction, nullptr) == 0;
        }
#endif
    }

    FScopedIgnoredSigPwrForTest::~FScopedIgnoredSigPwrForTest()
    {
#if PLATFORM_LINUX && defined(SIGPWR)
        if (bRestoreSigPwr)
        {
            sigaction(SIGPWR, &PreviousSigPwrAction, nullptr);
        }
#endif
    }

    FScopedAutomationBridgeSettingsOverride::FScopedAutomationBridgeSettingsOverride()
    {
        if (GConfig == nullptr)
        {
            return;
        }
        bHadNativeMcpEnabled = GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bPreviousNativeMcpEnabled, GGameIni);
        bHadNativeMcpPort = GConfig->GetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), PreviousNativeMcpPort, GGameIni);
        bHadListenHost = GConfig->GetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), PreviousListenHost, GGameIni);
        bHadRequireCapabilityToken = GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bPreviousRequireCapabilityToken, GGameIni);
        bHadCapabilityToken = GConfig->GetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), PreviousCapabilityToken, GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), true, GGameIni);
        GConfig->SetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), 43123, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), TEXT("0.0.0.0"), GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), true, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), TEXT("fake-capability-token"), GGameIni);
    }

    FScopedAutomationBridgeSettingsOverride::~FScopedAutomationBridgeSettingsOverride()
    {
        if (GConfig == nullptr)
        {
            return;
        }
        if (bHadNativeMcpEnabled)
        {
            GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bPreviousNativeMcpEnabled, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), GGameIni);
        }
        if (bHadNativeMcpPort)
        {
            GConfig->SetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), PreviousNativeMcpPort, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), GGameIni);
        }
        if (bHadListenHost)
        {
            GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), *PreviousListenHost, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), GGameIni);
        }
        if (bHadRequireCapabilityToken)
        {
            GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bPreviousRequireCapabilityToken, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), GGameIni);
        }
        if (bHadCapabilityToken)
        {
            GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), *PreviousCapabilityToken, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), GGameIni);
        }
    }

    bool PumpClientUntil(FOpenCodeAcpClient& Client, TFunctionRef<bool()> Predicate, double TimeoutSeconds)
    {
        const double StartedAt = FPlatformTime::Seconds();
        while (FPlatformTime::Seconds() - StartedAt < TimeoutSeconds)
        {
            Client.Tick();
            if (Predicate())
            {
                return true;
            }
            FPlatformProcess::Sleep(0.01f);
        }
        Client.Tick();
        return Predicate();
    }

    bool ContainsTranscript(const TArray<FString>& Entries, const FString& ExpectedText)
    {
        return Entries.ContainsByPredicate([&ExpectedText](const FString& Entry)
        {
            return Entry.Contains(ExpectedText);
        });
    }
}

#endif
