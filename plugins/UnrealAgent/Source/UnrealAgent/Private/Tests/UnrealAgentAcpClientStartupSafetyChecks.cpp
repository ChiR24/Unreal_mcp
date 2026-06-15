#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpClientProtocolChecks.h"

#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunAcpClientStartupSafetyChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
        const FString UnsafeStartDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpUnsafeStartHarness")));
        IFileManager::Get().DeleteDirectory(*UnsafeStartDirectory, false, true);
        const FString UnsafeStartConfigPath = FPaths::Combine(UnsafeStartDirectory, TEXT(".opencode/opencode.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(UnsafeStartConfigPath), true);
        bPassed &= Test.TestTrue(
            TEXT("Unsafe startup config is seeded"),
            FFileHelper::SaveStringToFile(
                TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\"*\": \"allow\"}\n}\n"),
                *UnsafeStartConfigPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FOpenCodeAcpClient UnsafeStartClient;
        FString UnsafeStartStatus;
        UnsafeStartClient.OnStatus.BindLambda([&UnsafeStartStatus](const FString& Status) { UnsafeStartStatus = Status; });
        bPassed &= Test.TestFalse(TEXT("ACP startup fails for unsafe preserved permissions"), UnsafeStartClient.Start(UnsafeStartDirectory));
        bPassed &= Test.TestTrue(TEXT("Unsafe startup exposes permission safety failure"), UnsafeStartClient.GetLastStudioKitSummary().Contains(TEXT("permission safety failed")));
        bPassed &= Test.TestTrue(TEXT("Unsafe startup reports config preparation failure"), UnsafeStartStatus.Contains(TEXT("Failed to prepare Unreal Agent OpenCode config")));
        UnsafeStartClient.Stop();
        IFileManager::Get().DeleteDirectory(*UnsafeStartDirectory, false, true);

        const FString UntrustedPluginDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpUntrustedPluginHarness")));
        IFileManager::Get().DeleteDirectory(*UntrustedPluginDirectory, false, true);
        FUnrealAgentStudioKit::EnsureForProject(UntrustedPluginDirectory);
        const FString UntrustedPluginPath = FPaths::Combine(
            UntrustedPluginDirectory,
            TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
        bPassed &= Test.TestTrue(
            TEXT("Modified guardrail plugin is seeded"),
            FFileHelper::SaveStringToFile(
                TEXT("export const UnrealAgentGuardrails = async () => ({})\n"),
                *UntrustedPluginPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FOpenCodeAcpClient UntrustedPluginClient;
        FString UntrustedPluginStatus;
        UntrustedPluginClient.OnStatus.BindLambda(
            [&UntrustedPluginStatus](const FString& Status)
            {
                UntrustedPluginStatus = Status;
            });
        bPassed &= Test.TestFalse(
            TEXT("ACP startup refuses a modified guardrail plugin"),
            UntrustedPluginClient.Start(UntrustedPluginDirectory));
        bPassed &= Test.TestTrue(
            TEXT("Modified guardrail refusal reports config preparation failure"),
            UntrustedPluginStatus.Contains(TEXT("Failed to prepare Unreal Agent OpenCode config")));
        UntrustedPluginClient.Stop();
        IFileManager::Get().DeleteDirectory(*UntrustedPluginDirectory, false, true);
        return bPassed;
    }
}

#endif
