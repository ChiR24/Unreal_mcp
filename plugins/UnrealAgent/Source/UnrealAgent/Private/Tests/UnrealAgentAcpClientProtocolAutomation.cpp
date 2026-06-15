#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAutomationTestDelegates.h"
#include "Tests/UnrealAgentAcpClientProtocolChecks.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientProtocolTest(FAutomationTestBase& Test)
    {
        bool bPassed = true;
        bPassed &= Test.TestEqual(TEXT("Raw IPv6 loopback host is URL-bracketed"), OpenCodeAcp::NormalizeMcpHostForUrl(TEXT("::1"), false), FString(TEXT("[::1]")));
        bPassed &= Test.TestEqual(TEXT("Wildcard host falls back to IPv4 loopback"), OpenCodeAcp::NormalizeMcpHostForUrl(TEXT("0.0.0.0"), true), FString(TEXT("127.0.0.1")));
        bPassed &= Test.TestEqual(TEXT("Non-loopback host falls back without explicit opt-in"), OpenCodeAcp::NormalizeMcpHostForUrl(TEXT("attacker.invalid"), false), FString(TEXT("127.0.0.1")));
        bPassed &= Test.TestEqual(TEXT("Non-loopback host is retained with explicit opt-in"), OpenCodeAcp::NormalizeMcpHostForUrl(TEXT("bridge.example"), true), FString(TEXT("bridge.example")));
        bPassed &= Test.TestTrue(TEXT("ACP MCP injection accepts a bridge configuration normalized to loopback"), OpenCodeAcp::CanInjectMcpServerForAcp(TEXT("attacker.invalid"), false, true, TEXT("test-capability-token")));
        bPassed &= Test.TestFalse(TEXT("ACP MCP injection rejects an opted-in non-loopback endpoint"), OpenCodeAcp::CanInjectMcpServerForAcp(TEXT("bridge.example"), true, true, TEXT("test-capability-token")));
        bPassed &= Test.TestFalse(TEXT("ACP MCP injection rejects an endpoint without required token auth"), OpenCodeAcp::CanInjectMcpServerForAcp(TEXT("127.0.0.1"), false, false, TEXT("test-capability-token")));
        bPassed &= Test.TestFalse(TEXT("ACP MCP injection rejects an empty capability token"), OpenCodeAcp::CanInjectMcpServerForAcp(TEXT("127.0.0.1"), false, true, FString()));

#if !(PLATFORM_LINUX || PLATFORM_MAC)
        Test.AddInfo(TEXT("Skipping ACP test server process harness on this platform."));
        return bPassed;
#else
        if (!FPaths::FileExists(TEXT("/usr/bin/python3")))
        {
            Test.AddInfo(TEXT("Skipping ACP test server process harness because /usr/bin/python3 is unavailable."));
            return true;
        }

        const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpTestServerHarness")));
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
        {
            Test.AddError(FString::Printf(TEXT("Failed to create ACP test harness directory: %s"), *TestDirectory));
            return false;
        }
        FScopedOpenCodeConfigEnvironment ConfigEnvironment(TestDirectory);

        const FString ScriptPath = FPaths::Combine(TestDirectory, TEXT("acptest"));
        Test.TestEqual(TEXT("ACP harness executable uses the acptest filename"), FPaths::GetCleanFilename(ScriptPath), FString(TEXT("acptest")));
        if (!FFileHelper::SaveStringToFile(MakeAcpTestServerScript(), *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            Test.AddError(FString::Printf(TEXT("Failed to write ACP test server script: %s"), *ScriptPath));
            return false;
        }

        int32 ChmodReturnCode = 1;
        FString ChmodOutput;
        FPlatformProcess::ExecProcess(TEXT("/bin/chmod"), *FString::Printf(TEXT("+x \"%s\""), *ScriptPath), &ChmodReturnCode, &ChmodOutput, &ChmodOutput);
        if (ChmodReturnCode != 0)
        {
            Test.AddError(FString::Printf(TEXT("Failed to make ACP test server script executable: %s"), *ChmodOutput));
            return false;
        }

        const FString PreviousOpenCodeCommand = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_ACP_COMMAND"));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *ScriptPath);
        FScopedAutomationBridgeSettingsOverride BridgeSettings;
        FOpenCodeAcpClient Client;
        FString LastStatus;
        FString LastPermission;
        TArray<FString> TranscriptEntries;
        int32 ModelChangeCount = 0;
        int32 StopCount = 0;
        Client.OnStatus.BindLambda([&LastStatus](const FString& Status) { LastStatus = Status; });
        Client.OnTranscript.BindLambda([&TranscriptEntries](const FString& Role, const FString& Text) { TranscriptEntries.Add(Role + TEXT(":") + Text); });
        Client.OnPermission.BindLambda([&LastPermission](const FString& Description) { LastPermission = Description; });
        Client.OnModelsChanged.BindLambda([&ModelChangeCount]() { ++ModelChangeCount; });
        Client.OnStopped.BindLambda([&StopCount]() { ++StopCount; });

        bool bClientStarted = false;
        {
            FScopedIgnoredSigPwrForTest IgnoredSigPwr;
            bClientStarted = Client.Start(TestDirectory);
        }
        if (Test.TestTrue(TEXT("ACP client starts against test server with ignored parent SIGPWR"), bClientStarted))
        {
            FAcpClientProtocolTestContext Context{
                Test,
                Client,
                TestDirectory,
                LastStatus,
                LastPermission,
                TranscriptEntries,
                ModelChangeCount
            };
            bPassed &= RunAcpClientSessionChecks(Context);
            bPassed &= RunAcpClientLocalAccessChecks(Context);
            bPassed &= RunAcpClientPermissionBehaviorChecks(Context);
            bPassed &= RunAcpClientMalformedPermissionChecks(Context);
        }
        else
        {
            bPassed = false;
        }

        Client.Stop();
        bPassed &= Test.TestEqual(TEXT("Stopped callback fires only when an active client stops"), StopCount, bClientStarted ? 1 : 0);
        bPassed &= RunAcpClientStartupSafetyChecks(Test);
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *PreviousOpenCodeCommand);
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        return bPassed;
#endif
    }
}

#endif
