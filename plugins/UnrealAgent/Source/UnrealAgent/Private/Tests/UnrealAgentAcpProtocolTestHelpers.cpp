#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

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
        bHadAllowNonLoopback = GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bAllowNonLoopback"), bPreviousAllowNonLoopback, GGameIni);
        bHadRequireCapabilityToken = GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bPreviousRequireCapabilityToken, GGameIni);
        bHadCapabilityToken = GConfig->GetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), PreviousCapabilityToken, GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), true, GGameIni);
        GConfig->SetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), 43123, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), TEXT("attacker.invalid"), GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bAllowNonLoopback"), false, GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), true, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), TEXT("test-capability-token"), GGameIni);
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
        if (bHadAllowNonLoopback)
        {
            GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bAllowNonLoopback"), bPreviousAllowNonLoopback, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("bAllowNonLoopback"), GGameIni);
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

    FScopedOpenCodeConfigEnvironment::FScopedOpenCodeConfigEnvironment(
        const FString& RootDirectory)
        : PreviousConfigDirectory(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG_DIR")))
        , PreviousConfig(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG")))
        , PreviousInlineConfig(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG_CONTENT")))
        , PreviousHome(FPlatformMisc::GetEnvironmentVariable(TEXT("HOME")))
        , PreviousXdgConfigHome(FPlatformMisc::GetEnvironmentVariable(TEXT("XDG_CONFIG_HOME")))
        , PreviousPermission(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_PERMISSION")))
        , PreviousDisableProjectConfig(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_DISABLE_PROJECT_CONFIG")))
        , PreviousPure(FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_PURE")))
        , PreviousAppData(FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA")))
        , PreviousLocalAppData(FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")))
    {
        const FString ConfigDirectory = FPaths::Combine(RootDirectory, TEXT("GlobalConfig"));
        const FString HomeDirectory = FPaths::Combine(RootDirectory, TEXT("Home"));
        const FString XdgDirectory = FPaths::Combine(RootDirectory, TEXT("XdgConfig"));
        IFileManager::Get().MakeDirectory(*ConfigDirectory, true);
        IFileManager::Get().MakeDirectory(*HomeDirectory, true);
        IFileManager::Get().MakeDirectory(*XdgDirectory, true);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_DIR"), *ConfigDirectory);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_CONTENT"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("HOME"), *HomeDirectory);
        FPlatformMisc::SetEnvironmentVar(TEXT("XDG_CONFIG_HOME"), *XdgDirectory);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PERMISSION"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PURE"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("APPDATA"), TEXT(""));
        FPlatformMisc::SetEnvironmentVar(TEXT("LOCALAPPDATA"), TEXT(""));
    }

    FScopedOpenCodeConfigEnvironment::~FScopedOpenCodeConfigEnvironment()
    {
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_DIR"), *PreviousConfigDirectory);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG"), *PreviousConfig);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_CONTENT"), *PreviousInlineConfig);
        FPlatformMisc::SetEnvironmentVar(TEXT("HOME"), *PreviousHome);
        FPlatformMisc::SetEnvironmentVar(TEXT("XDG_CONFIG_HOME"), *PreviousXdgConfigHome);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PERMISSION"), *PreviousPermission);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"), *PreviousDisableProjectConfig);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_PURE"), *PreviousPure);
        FPlatformMisc::SetEnvironmentVar(TEXT("APPDATA"), *PreviousAppData);
        FPlatformMisc::SetEnvironmentVar(TEXT("LOCALAPPDATA"), *PreviousLocalAppData);
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
